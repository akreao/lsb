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
#include "entities/char_entity.h"
#include "item_container.h"
#include "items/item.h"
#include "items/item_equipment.h"
#include "job_points.h"
#include "lua/luautils.h"
#include "merit.h"
#include "spell.h"
#include "zone.h"
#include "zoneutils.h"

#include <algorithm>
#include <atomic>
#include <cctype>
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

auto newBotId() -> uint32
{
    const uint32 id = nextBotId;
    nextBotId       = nextBotId >= BotIdLast ? BotIdFirst : nextBotId + 1;
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

auto MayHunt(CCharEntity* PBot) -> bool
{
    auto* PZone = PBot->loc.zone;
    if (PZone == nullptr)
    {
        return false;
    }

    int32 cap = settings::get<uint8>("map.PLAYER_BOTS_MAX_HUNTERS");
    if (settings::get<bool>("map.PLAYER_BOTS_HUNTERS_YIELD"))
    {
        PZone->ForEachChar(
            [&](CCharEntity* PChar)
            {
                if (PChar->PAI->IsEngaged())
                {
                    --cap;
                }
            });
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

    PRaw->loc.p           = spec.pos;
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

} // namespace botutils
