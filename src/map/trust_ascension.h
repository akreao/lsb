/*
===========================================================================

  Copyright (c) 2026 LandSandBoat Dev Teams

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

#include <array>

class CCharEntity;
class CTrustEntity;

// Trust Ascension: Alter Ego upgrades bought with alter ego points (Cipher Bracelet).
namespace trustascension
{

// Upgrade level per attribute, indexed by AlterEgoCategory as in s2c 0x08E. Stored as the alter_ego blob.
using AlterEgoLevels = std::array<uint8, 32>;

// The level cap of every attribute (client data: file 97 byte 3 = 0x3C).
constexpr uint8 MaxAlterEgoLevel = 60;

// The points the next level of an attribute at `level` costs: 1 up to level 9, 2 at 10..19, rising by one per
// ten levels (retail capture). 0 at the cap: the client shows no cost there and retail was not seen at the cap.
auto AlterEgoCost(uint8 level) -> uint16;

void Load(CCharEntity* PChar);
void SaveAlterEgo(const CCharEntity* PChar);

// Buys one level of an attribute (c2s 0x0C1). Returns false when the points or the cap do not allow it.
auto UpgradeAlterEgo(CCharEntity* PChar, uint8 category) -> bool;

// Adds the master's Alter Ego upgrades to a newly summoned trust.
void ApplyAlterEgo(CTrustEntity* PTrust, const CCharEntity* PMaster);

} // namespace trustascension
