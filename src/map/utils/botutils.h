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

#pragma once

#include "common/cbasetypes.h"
#include "common/mmo.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class CBaseEntity;
class CCharEntity;
class CZone;

// Player bots: server-made player shells that live only in memory, never in the database.
namespace botutils
{

struct BotSpec
{
    std::string                           name;
    uint8                                 race{ 1 };
    uint8                                 face{ 0 };
    uint8                                 size{ 0 };
    uint8                                 mainJob{ 1 };
    uint8                                 mainLevel{ 1 };
    uint8                                 subJob{ 0 };
    uint8                                 subLevel{ 0 };
    position_t                            pos{};
    std::vector<std::pair<uint8, uint16>> gear; // equip slot, item id
};

// Bot ids sit above every real character id and below the mob id range (0x01000000).
constexpr uint32 BotIdFirst = 0x00F00000;
constexpr uint32 BotIdLast  = 0x00FFFFFF;

// True for an id in the reserved bot range. The range is checked against the chars table before any
// bot spawns, so a real character never has one.
auto IsBotId(uint32 id) -> bool;

// True when the entity is a player bot.
auto IsBot(const CBaseEntity* PEntity) -> bool;

// Database writes seen with a bot id as a parameter since the map started. Bots must never write, so this
// stays 0; each one is also logged as an error.
auto GetBotDatabaseWrites() -> uint32;

// True when a bot of this job and level can wear the item in that equip slot (checks the gear file).
auto CanWear(uint16 itemId, uint8 job, uint8 level, uint8 equipSlot) -> bool;

// True while a live bot has this name, or a real character has it in the database.
auto IsNameTaken(const std::string& name) -> bool;

// Frees a despawned bot's name.
void ReleaseName(const std::string& name);

// Builds a bot and puts it in the zone. Returns nullptr when bots are off or the zone cannot take one.
auto SpawnBot(CZone* PZone, const BotSpec& spec) -> CCharEntity*;

} // namespace botutils
