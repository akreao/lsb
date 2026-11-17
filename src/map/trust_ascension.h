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

// Celestite: bit n of owned = celestite item CelestiteItemBase + n registered; sets[1..6] are the six combat types
// (1 Tank, 2 Melee Fighter, 3 Ranged Fighter, 4 Offensive Caster, 5 Healer, 6 Support), each with ten slot levels
// (0 = closed) then the ten celestites worn (item - CelestiteItemBase, 0 = empty). sets[0] is never read by the
// client. Laid out exactly as s2c 0x08F and stored as the celestite blob.
struct CelestiteData
{
    uint32 owned[8];
    uint8  sets[7][20];
};

constexpr uint16 CelestiteItemBase = 29424; // 0x72F0
constexpr uint8  CelestiteSets     = 6;
constexpr uint8  CelestiteSlots    = 10;

// NOT RETAIL. Unverified test values for the celestite effects by slot level 1/2/3, taken from a community test
// sheet (11/10..11/14/26). The client data names the effect of each celestite (item records: 1 Accuracy + Magic
// Accuracy, 2 Evasion + Magic Evasion, 3 Attack, 4 Defense, 5 Magic Attack, 6 Magic Defense) but holds no amounts.
// Only the four the sheet measured are set; the rest stay 0. Not applied yet: which set a summoned trust uses is
// still unknown.
struct UnverifiedCelestiteEffect
{
    uint8  celestite; // item - CelestiteItemBase
    uint16 bySlotLevel[3];
};

constexpr std::array<UnverifiedCelestiteEffect, 4> UnverifiedCelestiteEffects{ {
    { 1, { 50, 100, 150 } }, // Accuracy
    { 3, { 60, 120, 180 } }, // Attack
    { 4, { 70, 140, 210 } }, // Defense
    { 5, { 30, 40, 50 } },   // Magic Attack Bonus
} };

// The level cap of every attribute (client data: file 97 byte 3 = 0x3C).
constexpr uint8 MaxAlterEgoLevel = 60;

// The points the next level of an attribute at `level` costs: 1 up to level 9, 2 at 10..19, rising by one per
// ten levels (retail capture). 0 at the cap: the client shows no cost there and retail was not seen at the cap.
auto AlterEgoCost(uint8 level) -> uint16;

void Load(CCharEntity* PChar);
void SaveAlterEgo(const CCharEntity* PChar);
void SaveCelestite(const CCharEntity* PChar);

auto HasCelestite(const CCharEntity* PChar, uint16 itemId) -> bool;
void AddCelestite(CCharEntity* PChar, uint16 itemId);

// Sets one slot's level in all six sets (0 closes it).
void SetCelestiteSlotLevel(CCharEntity* PChar, uint8 slot, uint8 level);

// Replaces one set from c2s 0x0C5. Returns false (and changes nothing) when a celestite is not registered, sits in
// a closed slot or appears twice in the set.
auto SetCelestiteSet(CCharEntity* PChar, uint8 set, const uint8 (&slots)[CelestiteSlots]) -> bool;

// Buys one level of an attribute (c2s 0x0C1). Returns false when the points or the cap do not allow it.
auto UpgradeAlterEgo(CCharEntity* PChar, uint8 category) -> bool;

// Adds the master's Alter Ego upgrades to a newly summoned trust.
void ApplyAlterEgo(CTrustEntity* PTrust, const CCharEntity* PMaster);

} // namespace trustascension
