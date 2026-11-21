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

#include "charutils.h"
#include "itemutils.h"

#include "common/logging.h"
#include "common/settings.h"

#include "entities/char_entity.h"
#include "item_container.h"
#include "items/item.h"
#include "job_points.h"
#include "merit.h"
#include "zone.h"

namespace botutils
{

namespace
{

// Bot ids sit above every real character id and below the mob id range (0x01000000).
constexpr uint32 BotIdFirst = 0x00F00000;
constexpr uint32 BotIdLast  = 0x00FFFFFF;

uint32 nextBotId = BotIdFirst;

auto newBotId() -> uint32
{
    const uint32 id = nextBotId;
    nextBotId       = nextBotId >= BotIdLast ? BotIdFirst : nextBotId + 1;
    return id;
}

} // namespace

auto SpawnBot(CZone* PZone, const BotSpec& spec) -> CCharEntity*
{
    if (PZone == nullptr || !settings::get<bool>("map.PLAYER_BOTS_ENABLED"))
    {
        return nullptr;
    }

    if (spec.mainJob == 0 || spec.mainJob >= MAX_JOBTYPE || spec.subJob >= MAX_JOBTYPE || spec.mainLevel == 0)
    {
        ShowWarningFmt("botutils::SpawnBot: bad job or level for {}", spec.name);
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

    if (spec.subJob != 0)
    {
        PRaw->jobs.unlocked |= 1 << spec.subJob;
        PRaw->jobs.job[spec.subJob] = spec.subLevel;
        PRaw->SetSJob(spec.subJob);
        PRaw->SetSLevel(spec.subLevel);
    }

    // TODO: these read the database by char id (nothing is found for a bot id); give them an empty start.
    PRaw->PMeritPoints = std::make_unique<CMeritPoints>(PRaw);
    PRaw->PJobPoints   = std::make_unique<CJobPoints>(PRaw);

    auto* PInventory = PRaw->getStorage(LOC_INVENTORY);
    PInventory->SetSize(80);

    for (const auto& [equipSlot, itemId] : spec.gear)
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

    PRaw->health.hp = PRaw->GetMaxHP();
    PRaw->health.mp = PRaw->GetMaxMP();
    PRaw->UpdateHealth();

    PRaw->loc.p           = spec.pos;
    PRaw->loc.destination = PZone->GetID();
    PRaw->status          = xi::Status::Normal;
    PRaw->animation       = xi::Animation::None;

    return PZone->InsertBot(std::move(PBot));
}

} // namespace botutils
