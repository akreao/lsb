/*
===========================================================================

  Copyright (c) 2025 LandSandBoat Dev Teams

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see http://www.gnu.org/licenses/

===========================================================================
*/

#include "botutils.h"

#include "battleutils.h"
#include "charutils.h"
#include "itemutils.h"

#include "common/database.h"
#include "common/logging.h"
#include "common/settings.h"

#include "ai/ai_container.h"
#include "ai/controllers/bot_controller.h"
#include "ai/helpers/pathfind.h"
#include "enmity_container.h"
#include "entities/char_entity.h"
#include "entities/mob_entity.h"
#include "enums/chat_message_type.h"
#include "enums/msg_std.h"
#include "enums/party_kind.h"
#include "item_container.h"
#include "items/item.h"
#include "items/item_equipment.h"
#include "job_points.h"
#include "lua/lua_base_entity.h"
#include "lua/luautils.h"
#include "merit.h"
#include "packets/s2c/0x009_message.h"
#include "packets/s2c/0x017_chat_std.h"
#include "party.h"
#include "spell.h"
#include "status_effect_container.h"
#include "treasure_pool.h"
#include "ximesh/ximesh.h"
#include "zone.h"
#include "zoneutils.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace botutils
{

namespace
{

uint32 nextBotId = BotIdFirst;

std::atomic<uint32> botDatabaseWrites = 0;

// Names of live bots, so two bots never share one. Bots are supported on one map process only.
std::unordered_set<std::string> liveBotNames;

// What each live bot was built from, so it can come back the same after it dies.
std::unordered_map<uint32, BotSpec> liveBotSpecs;

// Zone entrances, worked out once per zone.
std::unordered_map<uint16, std::vector<position_t>> zoneEntrances;

// When each zone may next hear an idle line from a bot.
std::unordered_map<uint16, timer::time_point> nextIdleChat;

// `!bot pause`: no arrivals, no new hunts, until a GM resumes (memory only).
bool botsPaused = false;

// Bots each real player put on their blacklist this session (custom: bots have no char_blacklist row): char id ->
// lower-case bot name -> the id the client was given for it. Forgotten at logout.
std::unordered_map<uint32, std::unordered_map<std::string, uint32>> botBlacklists;

// Say lines reach this far, as a player's do.
constexpr float SayRange = 50.0f;

// A blacklist holds at most this many bots per player.
constexpr std::size_t MaxBlacklistedBots = 50;

auto lowerName(const std::string& name) -> std::string
{
    std::string lower = name;
    std::ranges::transform(lower, lower.begin(), [](const unsigned char c)
                           {
                               return static_cast<char>(std::tolower(c));
                           });
    return lower;
}

// Where real players fight, per zone and character, and until when it counts as their camp.
struct Camp
{
    position_t        pos{};
    timer::time_point until{};
};

std::unordered_map<uint16, std::unordered_map<uint32, Camp>> playerCamps;

// Each zone's mob level band (the middle half of its normal mobs' levels), worked out now and then.
struct LevelBand
{
    uint8             low{ 0 };
    uint8             high{ 0 };
    timer::time_point until{};
};

std::unordered_map<uint16, LevelBand> levelBands;

// Mob scripts (zone/name) that call xi.mob.phOnDespawn, so the mob is an NM's placeholder.
std::unordered_map<std::string, bool> placeholderScripts;

auto levelBand(CZone* PZone) -> const LevelBand&
{
    auto& band = levelBands[static_cast<uint16>(PZone->GetID())];
    if (timer::now() < band.until)
    {
        return band;
    }

    std::vector<uint8> levels;
    PZone->ForEachMob(
        [&](CMobEntity* PMob)
        {
            if (PMob->isAlive() && PMob->PAI->IsSpawned() && (PMob->m_Type & xi::MobType::Notorious) == xi::MobType::Normal && PMob->GetMLevel() > 0)
            {
                levels.push_back(PMob->GetMLevel());
            }
        });

    band = LevelBand{ .until = timer::now() + 5min };
    if (!levels.empty())
    {
        std::ranges::sort(levels);
        band.low  = levels[(levels.size() - 1) / 4];
        band.high = levels[(levels.size() - 1) * 3 / 4];
    }

    return band;
}

// A real player whose level fits the zone's mobs: the kind of player a hunting bot would crowd.
auto fitsZone(CZone* PZone, const CCharEntity* PChar) -> bool
{
    const auto& band = levelBand(PZone);
    return band.high != 0 && PChar->GetMLevel() + 2 >= band.low && PChar->GetMLevel() <= band.high + 8;
}

auto isPlaceholder(const CMobEntity* PMob) -> bool
{
    if (PMob->loc.zone == nullptr)
    {
        return false;
    }

    const auto key = fmt::format("{}/{}", PMob->loc.zone->getName(), PMob->getName());
    if (const auto it = placeholderScripts.find(key); it != placeholderScripts.end())
    {
        return it->second;
    }

    const bool placeholder  = IsPlaceholderScript(PMob->loc.zone->getName(), PMob->getName());
    placeholderScripts[key] = placeholder;
    return placeholder;
}

auto controllerOf(CCharEntity* PBot) -> CBotController*
{
    return dynamic_cast<CBotController*>(PBot->PAI->GetController());
}

auto newBotId() -> uint32
{
    // Skip ids a live bot still has (the counter wraps after 1M spawns).
    uint32 id = nextBotId;
    do
    {
        id        = nextBotId;
        nextBotId = nextBotId >= BotIdLast ? BotIdFirst : nextBotId + 1;
    } while (liveBotSpecs.contains(id));

    return id;
}

auto isWrite(const std::string_view query) -> bool
{
    const auto start = query.find_first_not_of(" \t\r\n(");
    if (start == std::string_view::npos)
    {
        return false;
    }

    for (const std::string_view verb : { "INSERT", "UPDATE", "DELETE", "REPLACE" })
    {
        if (query.size() - start >= verb.size() &&
            std::equal(verb.begin(), verb.end(), query.begin() + start, [](const char a, const char b)
                       {
                           return a == std::toupper(static_cast<unsigned char>(b));
                       }))
        {
            return true;
        }
    }

    return false;
}

// Watches every statement for a write that carries a bot id. Logs only: a real value (a gil amount, say)
// can fall in the bot range by chance, so blocking here could lose a real player's data.
void watchStatement(const db::Query query, const std::vector<db::BoundValue>& params)
{
    if (!isWrite(query.view()))
    {
        return;
    }

    for (const auto& param : params)
    {
        const bool hasBotId = std::visit(
            [](const auto& value) -> bool
            {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) >= 4)
                {
                    const auto number = static_cast<int64>(value);
                    return number >= BotIdFirst && number <= BotIdLast;
                }
                else
                {
                    return false;
                }
            },
            param);

        if (hasBotId)
        {
            ++botDatabaseWrites;
            ShowErrorFmt("Player bots: database write with a bot id: {}", query.view());
            return;
        }
    }
}

// Bots need ids no real character has. Checked once, before the first bot spawns.
auto idRangeIsFree() -> bool
{
    static const bool free = []
    {
        const auto rset = db::preparedStmt("SELECT charid FROM chars WHERE charid BETWEEN ? AND ? LIMIT 1", BotIdFirst, BotIdLast);
        if (!rset || rset->rowsCount() != 0)
        {
            ShowErrorFmt("Player bots: a character has an id in the bot id range 0x{:08X}-0x{:08X}; bots stay off", BotIdFirst, BotIdLast);
            return false;
        }

        // Bots live in one map process's memory; tells, parties and names would break across several.
        const auto zones = db::preparedStmt("SELECT COUNT(DISTINCT zoneip, zoneport) AS processes FROM zone_settings WHERE zoneport <> 0");
        if (zones && zones->rowsCount() != 0 && zones->next() && zones->get<uint32>("processes") > 1)
        {
            ShowErrorFmt("Player bots: more than one map process is configured (zone_settings); bots stay off");
            return false;
        }

        db::setStatementObserver(&watchStatement);
        return true;
    }();

    return free;
}

// Bot lines go to the chat audit like a player's, with the type marked BOT_ so nobody mistakes them for a person.
void AuditChat(const CCharEntity* PBot, const ChatChannel channel, const std::string& text, const CCharEntity* PTarget)
{
    if (!settings::get<bool>("map.AUDIT_CHAT"))
    {
        return;
    }

    const char* type    = "BOT_SAY";
    const char* setting = "map.AUDIT_SAY";
    switch (channel)
    {
        case ChatChannel::Tell:
            type    = "BOT_TELL";
            setting = "map.AUDIT_TELL";
            break;
        case ChatChannel::Party:
            type    = "BOT_PARTY";
            setting = "map.AUDIT_PARTY";
            break;
        case ChatChannel::Say:
            break;
    }

    if (!settings::get<bool>(setting))
    {
        return;
    }

    const auto  zoneId    = PBot->loc.zone != nullptr ? static_cast<uint16>(PBot->loc.zone->GetID()) : uint16{ 0 };
    const auto& recipient = PTarget != nullptr ? PTarget->getName() : std::string{};
    if (!db::preparedStmt("INSERT INTO audit_chat (speaker, type, zoneid, recipient, message, datetime) VALUES(?, ?, ?, NULLIF(?, ''), ?, current_timestamp())",
                          PBot->getName(),
                          type,
                          zoneId,
                          recipient,
                          text))
    {
        ShowErrorFmt("Player bots: failed to audit a line from {}", PBot->getName());
    }
}

} // namespace

auto IsBotId(const uint32 id) -> bool
{
    return id >= BotIdFirst && id <= BotIdLast;
}

auto IsBot(const CBaseEntity* PEntity) -> bool
{
    if (PEntity == nullptr || PEntity->objtype != TYPE_PC)
    {
        return false;
    }

    return static_cast<const CCharEntity*>(PEntity)->isBot();
}

auto CanWear(const uint16 itemId, const uint8 job, const uint8 level, const uint8 equipSlot) -> bool
{
    const auto* PItem = xi::items::lookup<CItemEquipment>(itemId);
    if (PItem == nullptr || job == 0 || job >= MAX_JOBTYPE || equipSlot >= SLOT_LINK1)
    {
        return false;
    }

    return PItem->getReqLvl() <= level &&
           (PItem->getJobs() & (1 << (job - 1))) != 0 &&
           (PItem->getEquipSlotId() & (1 << equipSlot)) != 0;
}

auto IsNameTaken(const std::string& name) -> bool
{
    if (liveBotNames.contains(name))
    {
        return true;
    }

    const auto rset = db::preparedStmt("SELECT charid FROM chars WHERE charname = ? LIMIT 1", name);
    return rset && rset->rowsCount() != 0;
}

// Queries must be written into the code, so the tables are listed here; CheckDatabaseForBots warns about any
// char table with a charid column that this list misses.
struct BotRowCheck
{
    std::string_view table;
    db::Query        query;
};

const auto botRowChecks = std::to_array<BotRowCheck>({
    { "char_chocobos", db::Query("SELECT COUNT(*) AS n FROM char_chocobos WHERE charid BETWEEN ? AND ?") },
    { "char_effects", db::Query("SELECT COUNT(*) AS n FROM char_effects WHERE charid BETWEEN ? AND ?") },
    { "char_equip", db::Query("SELECT COUNT(*) AS n FROM char_equip WHERE charid BETWEEN ? AND ?") },
    { "char_equip_saved", db::Query("SELECT COUNT(*) AS n FROM char_equip_saved WHERE charid BETWEEN ? AND ?") },
    { "char_exp", db::Query("SELECT COUNT(*) AS n FROM char_exp WHERE charid BETWEEN ? AND ?") },
    { "char_fishing_contest_history", db::Query("SELECT COUNT(*) AS n FROM char_fishing_contest_history WHERE charid BETWEEN ? AND ?") },
    { "char_flags", db::Query("SELECT COUNT(*) AS n FROM char_flags WHERE charid BETWEEN ? AND ?") },
    { "char_history", db::Query("SELECT COUNT(*) AS n FROM char_history WHERE charid BETWEEN ? AND ?") },
    { "char_inventory", db::Query("SELECT COUNT(*) AS n FROM char_inventory WHERE charid BETWEEN ? AND ?") },
    { "char_job_points", db::Query("SELECT COUNT(*) AS n FROM char_job_points WHERE charid BETWEEN ? AND ?") },
    { "char_jobs", db::Query("SELECT COUNT(*) AS n FROM char_jobs WHERE charid BETWEEN ? AND ?") },
    { "char_look", db::Query("SELECT COUNT(*) AS n FROM char_look WHERE charid BETWEEN ? AND ?") },
    { "char_merit", db::Query("SELECT COUNT(*) AS n FROM char_merit WHERE charid BETWEEN ? AND ?") },
    { "char_monstrosity", db::Query("SELECT COUNT(*) AS n FROM char_monstrosity WHERE charid BETWEEN ? AND ?") },
    { "char_pet", db::Query("SELECT COUNT(*) AS n FROM char_pet WHERE charid BETWEEN ? AND ?") },
    { "char_points", db::Query("SELECT COUNT(*) AS n FROM char_points WHERE charid BETWEEN ? AND ?") },
    { "char_profile", db::Query("SELECT COUNT(*) AS n FROM char_profile WHERE charid BETWEEN ? AND ?") },
    { "char_recast", db::Query("SELECT COUNT(*) AS n FROM char_recast WHERE charid BETWEEN ? AND ?") },
    { "char_skills", db::Query("SELECT COUNT(*) AS n FROM char_skills WHERE charid BETWEEN ? AND ?") },
    { "char_spells", db::Query("SELECT COUNT(*) AS n FROM char_spells WHERE charid BETWEEN ? AND ?") },
    { "char_stats", db::Query("SELECT COUNT(*) AS n FROM char_stats WHERE charid BETWEEN ? AND ?") },
    { "char_storage", db::Query("SELECT COUNT(*) AS n FROM char_storage WHERE charid BETWEEN ? AND ?") },
    { "char_style", db::Query("SELECT COUNT(*) AS n FROM char_style WHERE charid BETWEEN ? AND ?") },
    { "char_trust_ascension", db::Query("SELECT COUNT(*) AS n FROM char_trust_ascension WHERE charid BETWEEN ? AND ?") },
    { "char_unlocks", db::Query("SELECT COUNT(*) AS n FROM char_unlocks WHERE charid BETWEEN ? AND ?") },
    { "char_vars", db::Query("SELECT COUNT(*) AS n FROM char_vars WHERE charid BETWEEN ? AND ?") },
    { "char_world_passes", db::Query("SELECT COUNT(*) AS n FROM char_world_passes WHERE charid BETWEEN ? AND ?") },
    { "chars", db::Query("SELECT COUNT(*) AS n FROM chars WHERE charid BETWEEN ? AND ?") },
});

auto CheckDatabaseForBots() -> uint32
{
    const auto rset = db::preparedStmt("SELECT TABLE_NAME FROM information_schema.COLUMNS "
                                       "WHERE TABLE_SCHEMA = DATABASE() AND COLUMN_NAME = 'charid' AND TABLE_NAME LIKE 'char%'");
    while (rset && rset->next())
    {
        const auto table = rset->get<std::string>("TABLE_NAME");
        if (std::ranges::none_of(botRowChecks, [&](const BotRowCheck& check)
                                 {
                                     return check.table == table;
                                 }))
        {
            ShowWarningFmt("Player bots: table {} has a charid column but is not in the bot row check", table);
        }
    }

    uint32 total = 0;
    for (const auto& [table, query] : botRowChecks)
    {
        const auto count = db::preparedStmt(db::selected(query), BotIdFirst, BotIdLast);
        if (count && count->next())
        {
            if (const auto rows = count->get<uint32>("n"); rows != 0)
            {
                ShowErrorFmt("Player bots: {} row(s) with a bot id in {}", rows, table);
                total += rows;
            }
        }
    }
    if (total != 0)
    {
        const auto alert = fmt::format("[Player bots] {} database row(s) carry a bot id. Check the map log.", total);
        zoneutils::ForEachZone(
            [&](CZone* PZone)
            {
                PZone->ForEachChar(
                    [&](CCharEntity* PChar)
                    {
                        if (PChar->m_GMlevel > 0)
                        {
                            PChar->pushPacket<GP_SERV_COMMAND_CHAT_STD>(PChar, MESSAGE_SYSTEM_1, alert, "Server");
                        }
                    });
            });
    }

    return total;
}

void DailyDatabaseCheck(const timer::time_point tick)
{
    static timer::time_point next{};
    if (next == timer::time_point{})
    {
        next = tick + 5min; // not during start-up
        return;
    }

    if (tick < next)
    {
        return;
    }

    next        = tick + 24h;
    std::ignore = CheckDatabaseForBots();
}

void Despawn(CCharEntity* PBot)
{
    if (PBot != nullptr && PBot->loc.zone != nullptr)
    {
        PBot->loc.zone->DespawnBot(PBot->targid);
    }
}

void ResetForTests()
{
    zoneutils::ForEachZone(
        [](CZone* PZone)
        {
            PZone->DespawnBots();
        });

    liveBotNames.clear();
    liveBotSpecs.clear();
    zoneEntrances.clear();
    nextIdleChat.clear();
    botBlacklists.clear();
    botsPaused = false;
    playerCamps.clear();
    levelBands.clear();
    placeholderScripts.clear();
    nextBotId = BotIdFirst;

    if (const auto reset = lua["xi"]["bots"]["reset"]; reset.valid())
    {
        std::ignore = reset.get<sol::protected_function>()();
    }
}

void UpdateSpec(const CCharEntity* PBot)
{
    const auto it = liveBotSpecs.find(PBot->id);
    if (it == liveBotSpecs.end())
    {
        return;
    }

    auto& spec     = it->second;
    spec.mainLevel = PBot->jobs.job[spec.mainJob];
    if (spec.subJob != 0)
    {
        spec.subLevel = std::min<uint8>(PBot->jobs.job[spec.subJob], spec.mainLevel / 2);
    }
}

void Release(const uint32 botId, const std::string& name)
{
    liveBotNames.erase(name);
    liveBotSpecs.erase(botId);
}

auto GetSpec(const uint32 botId) -> const BotSpec*
{
    const auto it = liveBotSpecs.find(botId);
    return it != liveBotSpecs.end() ? &it->second : nullptr;
}

auto GetEntrances(CZone* PZone) -> const std::vector<position_t>&
{
    const auto zoneId = static_cast<uint16>(PZone->GetID());
    if (const auto it = zoneEntrances.find(zoneId); it != zoneEntrances.end())
    {
        return it->second;
    }

    auto& entrances = zoneEntrances[zoneId];
    zoneutils::ForEachZone(
        [&](CZone* POther)
        {
            for (const auto* PLine : POther->GetZoneLines())
            {
                if (static_cast<uint16>(PLine->destinationZoneId) == zoneId)
                {
                    entrances.push_back(PLine->destinationPos);
                }
            }
        });

    // Zones reached only by boat, airship or teleport: fall back to the zone's own zone lines.
    if (entrances.empty())
    {
        for (const auto* PLine : PZone->GetZoneLines())
        {
            entrances.push_back(PLine->originPos);
        }
    }

    return entrances;
}

auto GroundBelow(CZone* PZone, const position_t& pos, const float top, const float bottom) -> Maybe<float>
{
    if (PZone == nullptr || PZone->xiMesh() == nullptr || bottom <= top)
    {
        return std::nullopt;
    }

    // Down is +y: the ray runs from top down to bottom and stops at the first surface (invisible walls skipped).
    const auto hit = PZone->xiMesh()->getPositionInfo(Vector3{ pos.x, top, pos.z }, YOffsets{ 0.0f, bottom - top });
    if (!hit.has_value())
    {
        return std::nullopt;
    }

    return top + hit->fraction * (bottom - top);
}

auto GroundAbove(CZone* PZone, const position_t& pos, const float top, const float bottom) -> Maybe<float>
{
    if (PZone == nullptr || PZone->xiMesh() == nullptr || bottom <= top)
    {
        return std::nullopt;
    }

    // The same ray cast upward, from bottom to top: the first face it meets is the lowest one in the span.
    const auto hit = PZone->xiMesh()->getPositionInfo(Vector3{ pos.x, bottom, pos.z }, YOffsets{ 0.0f, top - bottom });
    if (!hit.has_value())
    {
        return std::nullopt;
    }

    return bottom + hit->fraction * (top - bottom);
}

void StandOnGround(CZone* PZone, position_t& pos)
{
    if (PZone == nullptr)
    {
        return;
    }

    // The height comes from the navmesh, the floor a player can walk on, never from the bare collision mesh (whose
    // first face under a point can be a rock, a roof or a canopy). A path point lies over the navmesh floor it was
    // built on, and its height runs straight between two corners; the navmesh poly straight over or under it nearest
    // that height is the floor there, so the bot follows each slope and keeps to its own level of a bridge or a ledge.
    // A point off the navmesh (a zone line's landing spot, a leader standing where the navmesh does not reach) slides
    // onto a floor close by, or keeps its place when the nearest one is far.
    constexpr float StepSlack        = 0.1f;
    constexpr float MaxSnap          = 3.0f;
    constexpr float NavFloorError    = 1.2f;
    constexpr float NavFloorFallback = 2.5f;
    if (auto* PNavMesh = PZone->navMesh())
    {
        position_t snapped = pos;
        PNavMesh->snapToValidPosition(snapped);

        const float slide = ::distance(snapped, pos, true);
        if (slide <= StepSlack)
        {
            pos.y = snapped.y;
        }
        else if (slide <= MaxSnap)
        {
            pos.x = snapped.x;
            pos.y = snapped.y;
            pos.z = snapped.z;
        }
    }

    // The navmesh is the floor to within its build error (a cell height up, a detail error of a yalm either way). Stand
    // on the collision face nearest to it, as a player's client stands its own character: the first face below within
    // that error, else the lowest above. A face further off is another floor (a ledge below, a rock or a roof above).
    if (const auto below = GroundBelow(PZone, pos, pos.y, pos.y + NavFloorError))
    {
        pos.y = *below;
    }
    else if (const auto above = GroundAbove(PZone, pos, pos.y - NavFloorError, pos.y))
    {
        pos.y = *above;
    }
    else if (const auto further = GroundBelow(PZone, pos, pos.y, pos.y + NavFloorFallback))
    {
        // Where the navmesh drifts further from the floor (a coarse detail mesh over rough ground) the floor is the
        // next face down: a player's client would stand the bot there anyway.
        pos.y = *further;
    }
}

auto IsPlaceholderScript(const std::string& zoneName, const std::string& mobName) -> bool
{
    std::ifstream file(fmt::format("./scripts/zones/{}/mobs/{}.lua", zoneName, mobName));
    if (!file)
    {
        return false;
    }

    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return text.find("phOnDespawn") != std::string::npos;
}

auto RealPlayers(CZone* PZone) -> std::vector<CCharEntity*>
{
    std::vector<CCharEntity*> players;
    if (PZone == nullptr)
    {
        return players;
    }

    PZone->ForEachChar(
        [&](CCharEntity* PChar)
        {
            if (!PChar->m_isGMHidden && PChar->status != xi::Status::Disappear)
            {
                players.push_back(PChar);
            }
        });

    return players;
}

auto FitsPartyLevel(const uint8 highest, const uint8 level) -> bool
{
    const int16 gap = static_cast<int16>(highest) - static_cast<int16>(level);
    return gap <= settings::get<uint8>("map.PLAYER_BOTS_PARTY_LEVEL_ABOVE") && -gap <= settings::get<uint8>("map.PLAYER_BOTS_PARTY_LEVEL_BELOW");
}

auto FindSeeker(CZone* PZone, const CCharEntity* PInviter, const uint8 highest) -> CCharEntity*
{
    for (auto* PChar : RealPlayers(PZone))
    {
        if (PChar != PInviter && PChar->isSeekingParty() && PChar->PParty == nullptr && FitsPartyLevel(highest, PChar->GetMLevel()))
        {
            return PChar;
        }
    }

    return nullptr;
}

void TrackCamps(CZone* PZone)
{
    auto&      camps = playerCamps[static_cast<uint16>(PZone->GetID())];
    const auto now   = timer::now();
    const auto keep  = std::chrono::minutes(settings::get<uint8>("map.PLAYER_BOTS_CAMP_MINUTES"));

    for (auto* PChar : RealPlayers(PZone))
    {
        if (PChar->PAI->IsEngaged())
        {
            camps[PChar->id] = Camp{ .pos = PChar->loc.p, .until = now + keep };
        }
    }

    std::erase_if(camps, [now](const auto& entry)
                  {
                      return entry.second.until < now;
                  });
}

auto InPlayerCamp(CZone* PZone, const position_t& pos) -> bool
{
    const auto it = playerCamps.find(static_cast<uint16>(PZone->GetID()));
    if (it == playerCamps.end())
    {
        return false;
    }

    const auto  now   = timer::now();
    const float range = settings::get<float>("map.PLAYER_BOTS_CAMP_RANGE");
    return std::ranges::any_of(it->second, [&](const auto& entry)
                               {
                                   return entry.second.until >= now && ::distance(entry.second.pos, pos) < range;
                               });
}

auto IsFairPrey(CCharEntity* PBot, CMobEntity* PMob) -> bool
{
    if (PMob == nullptr || !PMob->isAlive() || !PMob->PAI->IsSpawned() || PMob->PAI->IsEngaged() || PMob->m_OwnerID.isSet() ||
        PMob->PMaster != nullptr || PMob->allegiance != xi::Allegiance::Mob || (PMob->m_Type & xi::MobType::Notorious) != xi::MobType::Normal ||
        PMob->m_SpawnType == xi::SpawnType::Scripted || PMob->m_SpawnType == xi::SpawnType::Lottery || !PMob->m_AllowRespawn ||
        PMob->m_RespawnTime <= 0s)
    {
        return false;
    }

    auto* PZone = PMob->loc.zone;
    if (PZone == nullptr || InPlayerCamp(PZone, PMob->loc.p) || isPlaceholder(PMob))
    {
        return false;
    }

    // Leave it to any real player close to it, or who has it targeted; never hunt near a player who turned bots off.
    constexpr float PlayerClearRange = 25.0f;
    const float     optOutRange      = settings::get<float>("map.PLAYER_BOTS_OPT_OUT_RANGE");
    for (const auto* PChar : RealPlayers(PZone))
    {
        const float range = ::distance(PChar->loc.p, PMob->loc.p);
        if (range < PlayerClearRange || PChar->m_TargID == PMob->targid || PChar->GetBattleTarget() == PMob ||
            (range < optOutRange && OptedOut(PChar)))
        {
            return false;
        }
    }

    return true;
}

void DropClaims(CCharEntity* PBot)
{
    if (PBot->loc.zone == nullptr)
    {
        return;
    }

    PBot->loc.zone->ForEachMob(
        [PBot](CMobEntity* PMob)
        {
            PMob->PEnmityContainer->Clear(PBot->id);
            if (PMob->m_OwnerID.UniqueNo == PBot->id)
            {
                PMob->m_OwnerID.clean();
                PMob->updatemask |= UPDATE_STATUS;
            }
        });
}

auto MayHunt(CCharEntity* PBot) -> bool
{
    auto* PZone = PBot->loc.zone;
    if (PZone == nullptr || botsPaused)
    {
        return false;
    }

    // Starting zones belong to new players: bots there roam and chat but never hunt.
    const auto zoneId = static_cast<uint16>(PZone->GetID());
    if (const sol::object noHunt = lua["xi"]["settings"]["map"]["PLAYER_BOTS_NO_HUNT_ZONES"]; noHunt.is<sol::table>())
    {
        for (const auto& [_, entry] : noHunt.as<sol::table>())
        {
            if (entry.is<uint16>() && entry.as<uint16>() == zoneId)
            {
                return false;
            }
        }
    }

    int32 cap = settings::get<uint8>("map.PLAYER_BOTS_MAX_HUNTERS");
    if (settings::get<bool>("map.PLAYER_BOTS_HUNTERS_YIELD"))
    {
        // One hunter fewer for each real player of the zone's level, fighting or not; none with three or more.
        int32 fitting = 0;
        for (const auto* PChar : RealPlayers(PZone))
        {
            fitting += fitsZone(PZone, PChar) ? 1 : 0;
        }

        cap = fitting >= 3 ? 0 : cap - fitting;
    }

    int32 hunting = 0;
    PZone->ForEachBot(
        [&](CCharEntity* POther)
        {
            if (POther != PBot)
            {
                if (const auto* PController = dynamic_cast<CBotController*>(POther->PAI->GetController()); PController && PController->IsHunting())
                {
                    ++hunting;
                }
            }
        });

    return hunting < cap;
}

void Populate(CZone* PZone)
{
    const auto populate = lua["xi"]["bots"]["populate"];
    if (botsPaused || !populate.valid())
    {
        return;
    }

    const auto result = populate.get<sol::protected_function>()(PZone);
    if (!result.valid())
    {
        const sol::error err = result;
        ShowErrorFmt("botutils::Populate: {}", err.what());
    }
}

auto GetBotDatabaseWrites() -> uint32
{
    return botDatabaseWrites.load();
}

auto SpawnBot(CZone* PZone, const BotSpec& spec) -> CCharEntity*
{
    if (PZone == nullptr || !settings::get<bool>("map.PLAYER_BOTS_ENABLED") || !idRangeIsFree())
    {
        return nullptr;
    }

    if (spec.mainJob == 0 || spec.mainJob >= MAX_JOBTYPE || spec.subJob >= MAX_JOBTYPE || spec.mainLevel == 0)
    {
        ShowWarningFmt("botutils::SpawnBot: bad job or level for {}", spec.name);
        return nullptr;
    }

    // A cap for the whole server, whatever each zone wants (respawns and followers leave before they come back).
    if (liveBotSpecs.size() >= settings::get<uint16>("map.PLAYER_BOTS_MAX_TOTAL"))
    {
        return nullptr;
    }

    if (IsNameTaken(spec.name))
    {
        ShowWarningFmt("botutils::SpawnBot: the name {} is taken", spec.name);
        return nullptr;
    }

    auto  PBot = std::make_unique<CCharEntity>();
    auto* PRaw = PBot.get();

    PRaw->setBot();
    PRaw->id   = newBotId();
    PRaw->name = spec.name;

    PRaw->look.race = spec.race;
    PRaw->look.face = spec.face;
    PRaw->look.size = spec.size;

    PRaw->jobs.unlocked |= 1 << spec.mainJob;
    PRaw->jobs.job[spec.mainJob] = spec.mainLevel;
    PRaw->SetMJob(spec.mainJob);
    PRaw->SetMLevel(spec.mainLevel);

    // Battle entities start as WAR/WAR; a bot with no support job must say so, as a player's does.
    PRaw->SetSJob(spec.subJob);
    PRaw->SetSLevel(0);

    if (spec.subJob != 0)
    {
        PRaw->jobs.unlocked |= 1 << spec.subJob;
        PRaw->jobs.job[spec.subJob] = spec.subLevel;
        PRaw->SetSLevel(spec.subLevel);
    }

    // Skills at the cap for the bot's jobs and levels, as a player who kept them up would have them.
    for (uint8 i = static_cast<uint8>(xi::SkillType::HandToHand); i <= static_cast<uint8>(xi::SkillType::Handbell); ++i)
    {
        const auto   skill     = static_cast<xi::SkillType>(i);
        const uint16 mainSkill = battleutils::GetMaxSkill(skill, static_cast<xi::Job>(spec.mainJob), spec.mainLevel);
        const uint16 subSkill  = spec.subJob != 0 ? battleutils::GetMaxSkill(skill, static_cast<xi::Job>(spec.subJob), spec.subLevel) : uint16{ 0 };

        PRaw->RealSkills.skill[i] = 10 * std::max(mainSkill, subSkill);
    }

    // Every spell the jobs can cast at these levels (a player buys the scrolls as they level).
    for (uint16 spellId = 1; spellId < MAX_SPELL_ID; ++spellId)
    {
        auto* PSpell = spell::GetSpell(static_cast<SpellID>(spellId));
        if (PSpell == nullptr)
        {
            continue;
        }

        const uint8 mainLevel = PSpell->getJob(static_cast<xi::Job>(spec.mainJob));
        const uint8 subLevel  = spec.subJob != 0 ? PSpell->getJob(static_cast<xi::Job>(spec.subJob)) : 255;
        if ((mainLevel > 0 && mainLevel <= spec.mainLevel) || (subLevel > 0 && subLevel <= spec.subLevel))
        {
            charutils::addSpell(PRaw, spellId);
        }
    }

    // TODO: these read the database by char id (nothing is found for a bot id); give them an empty start.
    PRaw->PMeritPoints = std::make_unique<CMeritPoints>(PRaw);
    PRaw->PJobPoints   = std::make_unique<CJobPoints>(PRaw);

    auto* PInventory = PRaw->getStorage(LOC_INVENTORY);
    PInventory->SetSize(80);

    // Traits first: a second weapon needs Dual Wield. Then the main hand before the off hand.
    charutils::BuildingCharTraitsTable(PRaw);

    auto gear = spec.gear;
    std::ranges::sort(gear);

    for (const auto& [equipSlot, itemId] : gear)
    {
        auto PItem = xi::items::spawn(itemId);
        if (PItem == nullptr)
        {
            ShowWarningFmt("botutils::SpawnBot: unknown item {} for {}", itemId, spec.name);
            continue;
        }

        const uint8 slotId = PInventory->InsertItem(std::move(PItem));
        if (slotId == ERROR_SLOTID)
        {
            continue;
        }

        charutils::EquipItem(PRaw, slotId, equipSlot, LOC_INVENTORY);
    }

    std::memcpy(&PRaw->mainlook, &PRaw->look, sizeof(PRaw->look));

    charutils::CalculateStats(PRaw);
    charutils::BuildingCharSkillsTable(PRaw);
    charutils::BuildingCharAbilityTable(PRaw);
    charutils::BuildingCharTraitsTable(PRaw);
    charutils::BuildingCharWeaponSkills(PRaw);

    // GetMaxHP/GetMaxMP are only valid after UpdateHealth turns the new stats into max values.
    PRaw->UpdateHealth();
    PRaw->health.hp = PRaw->GetMaxHP();
    PRaw->health.mp = PRaw->GetMaxMP();

    PRaw->playerConfig.InviteFlg = spec.seeking;

    // The documented sign that this is a bot: a fixed line in the /check window (the bazaar comment), where a
    // player writes their own (custom). Its bazaar has nothing for sale.
    PRaw->bazaar.message = settings::get<std::string>("map.PLAYER_BOTS_CHECK_NOTE");

    PRaw->loc.p = spec.pos;
    StandOnGround(PZone, PRaw->loc.p);
    PRaw->loc.destination = PZone->GetID();
    PRaw->status          = xi::Status::Normal;
    PRaw->animation       = xi::Animation::None;

    // A bot walks and fights on its own: it needs a path finder and its own brain.
    PRaw->PAI->PathFind = std::make_unique<CPathFind>(PRaw);
    PRaw->PAI->SetController(std::make_unique<CBotController>(PRaw));

    auto* PInserted = PZone->InsertBot(std::move(PBot));
    if (PInserted != nullptr)
    {
        liveBotNames.insert(spec.name);
        liveBotSpecs[PInserted->id] = spec;

        // Every way a bot comes in (arrival, respawn, following its leader) gets its job's gambits here: spells,
        // cures and abilities all go through them.
        if (const auto applyGambits = lua["xi"]["bots"]["applyGambits"]; applyGambits.valid())
        {
            const auto result = applyGambits.get<sol::protected_function>()(CLuaBaseEntity(PInserted));
            if (!result.valid())
            {
                const sol::error err = result;
                ShowErrorFmt("botutils::SpawnBot: gambits for {}: {}", spec.name, err.what());
            }
        }
    }

    return PInserted;
}

void OnPartyInvite(CCharEntity* PBot, CCharEntity* PInviter)
{
    auto* PController = controllerOf(PBot);
    if (PController == nullptr || PBot->isDead() || PBot->InvitePending.entity.UniqueNo != 0)
    {
        PInviter->pushPacket<GP_SERV_COMMAND_MESSAGE>(PInviter, 0, 0, MsgStd::CannotInvite);
        return;
    }

    if (!PController->TakeInvite(PInviter->id))
    {
        return;
    }

    PBot->InvitePending.entity.UniqueNo = PInviter->id;
    PBot->InvitePending.entity.ActIndex = PInviter->targid;
    PBot->InvitePending.kind            = PartyKind::Party;
    PController->QueueInvite();
}

void LeaveParty(CCharEntity* PBot)
{
    auto* PParty = PBot->PParty;
    if (PParty == nullptr)
    {
        return;
    }

    const bool realMemberLeft = std::ranges::any_of(PParty->members, [PBot](const CBattleEntity* PMember)
                                                    {
                                                        return PMember != PBot && !IsBot(PMember);
                                                    });

    if (realMemberLeft)
    {
        // The normal path: the remaining members get a new party list, the bot gets its own loot pool.
        PParty->RemoveMember(PBot);
        return;
    }

    // Nobody real is left here (the leader zoned or logged out): the party ends for every bot still in it.
    // Not playerInitiated: the database never had these members, so no other process needs telling.
    PParty->DisbandParty(false);
}

auto MayFollowInto(const CCharEntity* PLeader) -> bool
{
    if (PLeader == nullptr || PLeader->loc.zone == nullptr || PLeader->inMogHouse() || !settings::get<bool>("map.PLAYER_BOTS_ENABLED"))
    {
        return false;
    }

    const auto isZoneAllowed = lua["xi"]["bots"]["isZoneAllowed"];
    if (!isZoneAllowed.valid())
    {
        return false;
    }

    const auto result = isZoneAllowed.get<sol::protected_function>()(PLeader->loc.zone);
    if (!result.valid())
    {
        const sol::error err = result;
        ShowErrorFmt("botutils::MayFollowInto: {}", err.what());
        return false;
    }

    return result.get<bool>();
}

void FollowLeader(CCharEntity* PBot, CCharEntity* PLeader)
{
    auto* PParty = PBot->PParty;
    if (PParty == nullptr || PLeader == nullptr || PLeader->PParty != PParty || PLeader->loc.zone == nullptr)
    {
        return;
    }

    UpdateSpec(PBot);
    const auto* PSpec = GetSpec(PBot->id);
    if (PSpec == nullptr)
    {
        return;
    }

    auto spec    = *PSpec;
    spec.seeking = false;
    spec.pos     = PLeader->loc.p;
    spec.pos.x += xirand::GetRandomNumber(-1.5f, 1.5f);
    spec.pos.z += xirand::GetRandomNumber(-1.5f, 1.5f);

    ShowDebugFmt("Player bots: {} follows {} to {}", PBot->getName(), PLeader->getName(), PLeader->loc.zone->getName());

    // Out of the party quietly (the leader is still in it, so it lives on); the leader's list updates once, when
    // the bot takes its seat again below.
    PParty->PopMember(PBot);
    PBot->PParty = nullptr;

    auto* PDestination = PLeader->loc.zone;
    Despawn(PBot);

    auto* PFollower = SpawnBot(PDestination, spec);
    if (PFollower == nullptr)
    {
        ShowWarningFmt("Player bots: {} could not follow {}", spec.name, PLeader->getName());
        PParty->ReloadParty();
        return;
    }

    PParty->AddMember(PFollower);
    if (PFollower->PParty == nullptr)
    {
        PParty->ReloadParty();
        return;
    }

    if (auto* PController = controllerOf(PFollower))
    {
        PController->JoinedByFollow(PLeader->id);
    }
}

auto FindBotByName(const std::string& name) -> CCharEntity*
{
    if (name.empty() || liveBotNames.empty())
    {
        return nullptr;
    }

    CCharEntity* PFound = nullptr;
    zoneutils::ForEachZone(
        [&](CZone* PZone)
        {
            if (PFound != nullptr || PZone->GetBotCount() == 0)
            {
                return;
            }

            PZone->ForEachBot(
                [&](CCharEntity* PBot)
                {
                    if (PFound == nullptr && strcmpi(PBot->getName().c_str(), name.c_str()) == 0)
                    {
                        PFound = PBot;
                    }
                });
        });

    return PFound;
}

void OnTell(CCharEntity* PBot, CCharEntity* PSender)
{
    if (auto* PController = controllerOf(PBot))
    {
        PController->QueueTellReply(PSender->id);
    }
}

void Chat(CCharEntity* PBot, const std::string& event, const ChatChannel channel, CCharEntity* PTarget)
{
    if (PBot == nullptr || !settings::get<bool>("map.PLAYER_BOTS_CHAT"))
    {
        return;
    }

    // Nothing for a player who blacklisted the bot or turned bots off (before a line is picked, so its cooldown
    // is not used up on nobody).
    if (channel == ChatChannel::Tell && (PTarget == nullptr || IgnoresBot(PTarget, PBot)))
    {
        return;
    }

    const auto chatLine = lua["xi"]["bots"]["chatLine"];
    if (!chatLine.valid())
    {
        return;
    }

    const auto result = chatLine.get<sol::protected_function>()(CLuaBaseEntity(PBot), event);
    if (!result.valid())
    {
        const sol::error err = result;
        ShowErrorFmt("botutils::Chat: {}", err.what());
        return;
    }

    const sol::object line = result;
    if (!line.is<std::string>() || line.as<std::string>().empty())
    {
        return;
    }

    const auto text = line.as<std::string>();
    switch (channel)
    {
        case ChatChannel::Tell:
            PTarget->pushPacket<GP_SERV_COMMAND_CHAT_STD>(PBot, MESSAGE_TELL, text);
            break;
        case ChatChannel::Party:
            if (PBot->PParty != nullptr)
            {
                PBot->PParty->PushPacket(PBot->id, xi::ZoneId::Unknown, std::make_unique<GP_SERV_COMMAND_CHAT_STD>(PBot, MESSAGE_PARTY, text));
            }
            break;
        case ChatChannel::Say:
            if (PBot->loc.zone != nullptr)
            {
                // Not to players who blacklisted the bot or turned bots off.
                const auto packet = std::make_unique<GP_SERV_COMMAND_CHAT_STD>(PBot, MESSAGE_SAY, text);
                PBot->loc.zone->ForEachChar(
                    [&](CCharEntity* PChar)
                    {
                        if (distance(PChar->loc.p, PBot->loc.p) <= SayRange && !IgnoresBot(PChar, PBot))
                        {
                            PChar->pushPacket(packet->copy());
                        }
                    });
            }
            break;
    }

    AuditChat(PBot, channel, text, PTarget);
}

void IdleChat(CZone* PZone)
{
    const auto interval = settings::get<uint16>("map.PLAYER_BOTS_SAY_INTERVAL");
    if (PZone == nullptr || interval == 0 || (PZone->GetTypeMask() & xi::ZoneType::City) == xi::ZoneType::Unknown)
    {
        return;
    }

    const auto zoneId = static_cast<uint16>(PZone->GetID());
    const auto now    = timer::now();
    auto&      next   = nextIdleChat[zoneId];
    if (next == timer::time_point{})
    {
        // Not the moment the first player arrives: start somewhere in the first interval.
        next = now + std::chrono::seconds(xirand::GetRandomNumber<int32>(interval / 2, interval + 1));
        return;
    }

    if (now < next)
    {
        return;
    }

    next = now + std::chrono::seconds(xirand::GetRandomNumber<int32>(interval / 2, interval * 3 / 2 + 1));

    std::vector<CCharEntity*> speakers;
    PZone->ForEachBot(
        [&](CCharEntity* PBot)
        {
            if (PBot->isAlive() && PBot->PParty == nullptr && !PBot->PAI->IsEngaged())
            {
                speakers.push_back(PBot);
            }
        });

    if (!speakers.empty())
    {
        Chat(speakers[xirand::GetRandomNumber(speakers.size())], "idle_city", ChatChannel::Say);
    }
}

auto IgnoresBot(const CCharEntity* PChar, const CCharEntity* PBot) -> bool
{
    if (PChar == nullptr || PBot == nullptr)
    {
        return false;
    }

    if (PChar->getCharVar(OptOutVar) > 0)
    {
        return true;
    }

    const auto it = botBlacklists.find(PChar->id);
    return it != botBlacklists.end() && it->second.contains(lowerName(PBot->getName()));
}

auto BlacklistBot(CCharEntity* PChar, const std::string& name) -> uint32
{
    const auto* PBot = FindBotByName(name);
    if (PChar == nullptr || PBot == nullptr)
    {
        return 0;
    }

    auto& list = botBlacklists[PChar->id];
    if (list.contains(lowerName(PBot->getName())) || list.size() >= MaxBlacklistedBots)
    {
        return 0;
    }

    list[lowerName(PBot->getName())] = PBot->id;
    return PBot->id;
}

auto UnblacklistBot(const CCharEntity* PChar, const std::string& name) -> uint32
{
    const auto it = botBlacklists.find(PChar->id);
    if (it == botBlacklists.end())
    {
        return 0;
    }

    const auto entry = it->second.find(lowerName(name));
    if (entry == it->second.end())
    {
        return 0;
    }

    const uint32 id = entry->second;
    it->second.erase(entry);
    return id;
}

auto BlacklistedBots(const CCharEntity* PChar) -> std::vector<std::pair<uint32, std::string>>
{
    std::vector<std::pair<uint32, std::string>> result;
    const auto                                  it = botBlacklists.find(PChar->id);
    if (it == botBlacklists.end())
    {
        return result;
    }

    for (const auto& [name, id] : it->second)
    {
        // The client shows the name as the bot spells it.
        const auto* PBot  = FindBotByName(name);
        auto        shown = PBot != nullptr ? PBot->getName() : name;
        if (PBot == nullptr && !shown.empty())
        {
            shown[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(shown[0])));
        }

        result.emplace_back(id, shown);
    }

    return result;
}

void ForgetPlayer(const uint32 charId)
{
    botBlacklists.erase(charId);
}

auto OptedOut(const CCharEntity* PChar) -> bool
{
    return PChar != nullptr && !IsBot(PChar) && PChar->getCharVar(OptOutVar) > 0;
}

auto GetLiveBotCount() -> std::size_t
{
    return liveBotSpecs.size();
}

auto ScatterPoint(CZone* PZone) -> Maybe<position_t>
{
    if (PZone == nullptr || PZone->navMesh() == nullptr)
    {
        return std::nullopt;
    }

    const auto& entrances = GetEntrances(PZone);
    if (entrances.empty())
    {
        return std::nullopt;
    }

    // Anywhere on the navmesh within reach of an entrance, out of the sight of every real player.
    constexpr float ScatterRadius = 200.0f;
    constexpr float OutOfSight    = 60.0f;
    const auto      players       = RealPlayers(PZone);
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        const auto& from  = entrances[xirand::GetRandomNumber(entrances.size())];
        const auto  point = PZone->navMesh()->findRandomPosition(from, ScatterRadius);
        if (!point)
        {
            continue;
        }

        const bool seen = std::ranges::any_of(players, [&](const CCharEntity* PChar)
                                              {
                                                  return ::distance(PChar->loc.p, *point) < OutOfSight;
                                              });
        if (!seen)
        {
            return point;
        }
    }

    return std::nullopt;
}

void OnLogout(CCharEntity* PBot)
{
    if (const auto onLogout = lua["xi"]["bots"]["onLogout"]; onLogout.valid())
    {
        const auto result = onLogout.get<sol::protected_function>()(CLuaBaseEntity(PBot));
        if (!result.valid())
        {
            const sol::error err = result;
            ShowErrorFmt("botutils::OnLogout: {}", err.what());
        }
    }
}

auto ListBots() -> std::vector<BotInfo>
{
    std::vector<BotInfo> bots;
    zoneutils::ForEachZone(
        [&](CZone* PZone)
        {
            if (PZone->GetBotCount() == 0)
            {
                return;
            }

            PZone->ForEachBot(
                [&](CCharEntity* PBot)
                {
                    BotInfo info;
                    info.id        = PBot->id;
                    info.name      = PBot->getName();
                    info.zoneId    = static_cast<uint16>(PZone->GetID());
                    info.zoneName  = PZone->getName();
                    info.mainJob   = static_cast<uint8>(PBot->GetMJob());
                    info.mainLevel = PBot->GetMLevel();
                    info.subJob    = static_cast<uint8>(PBot->GetSJob());
                    info.subLevel  = PBot->GetSLevel();
                    info.pos       = PBot->loc.p;

                    const auto* PController = dynamic_cast<CBotController*>(PBot->PAI->GetController());
                    if (PBot->isDead())
                    {
                        info.state = "dead";
                    }
                    else if (PBot->PAI->IsEngaged())
                    {
                        info.state = "fighting";
                    }
                    else if (PBot->PParty != nullptr)
                    {
                        info.state = "party";
                    }
                    else if (PController != nullptr && PController->IsHunting())
                    {
                        info.state = "hunting";
                    }
                    else if (PBot->StatusEffectContainer->HasStatusEffect(xi::StatusEffect::Healing))
                    {
                        info.state = "resting";
                    }
                    else if (PBot->playerConfig.InviteFlg)
                    {
                        info.state = "seeking";
                    }
                    else
                    {
                        info.state = "roaming";
                    }

                    if (PBot->PParty != nullptr)
                    {
                        if (const auto* PLeader = PBot->PParty->GetLeader())
                        {
                            info.partyWith = PLeader->getName();
                        }
                    }

                    if (PController != nullptr)
                    {
                        info.onlineSeconds = static_cast<uint32>(std::chrono::duration_cast<std::chrono::seconds>(timer::now() - PController->OnlineSince()).count());
                    }

                    bots.push_back(std::move(info));
                });
        });

    return bots;
}

void SetPaused(const bool paused)
{
    botsPaused = paused;
    ShowInfoFmt("Player bots: {}", paused ? "paused (no arrivals, no new hunts)" : "running again");
}

auto IsPaused() -> bool
{
    return botsPaused;
}

} // namespace botutils
