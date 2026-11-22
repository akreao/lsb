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

void SnapToGround(CZone* PZone, position_t& pos)
{
    if (PZone == nullptr || PZone->xiMesh() == nullptr)
    {
        return;
    }

    // Down is +y. Look from a little above the feet to a few yalms below them, so a ceiling overhead is not hit.
    constexpr float Above = 1.5f;
    constexpr float Below = 4.0f;

    const auto hit = PZone->xiMesh()->getPositionInfo(Vector3{ pos.x, pos.y, pos.z }, YOffsets{ -Above, Above + Below });
    if (hit.has_value() && !hit->barrier)
    {
        pos.y = pos.y - Above + hit->fraction * (Above + Below);
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

    // Leave it to any real player close to it, or who has it targeted.
    constexpr float PlayerClearRange = 25.0f;
    for (const auto* PChar : RealPlayers(PZone))
    {
        if (::distance(PChar->loc.p, PMob->loc.p) < PlayerClearRange || PChar->m_TargID == PMob->targid || PChar->GetBattleTarget() == PMob)
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
    if (PZone == nullptr)
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
    if (!populate.valid())
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

    PRaw->loc.p = spec.pos;
    SnapToGround(PZone, PRaw->loc.p);
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
            if (PTarget != nullptr)
            {
                PTarget->pushPacket<GP_SERV_COMMAND_CHAT_STD>(PBot, MESSAGE_TELL, text);
            }
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
                PBot->loc.zone->PushPacket(PBot, CHAR_INRANGE, std::make_unique<GP_SERV_COMMAND_CHAT_STD>(PBot, MESSAGE_SAY, text));
            }
            break;
    }
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

} // namespace botutils
