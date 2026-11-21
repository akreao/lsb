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

#pragma once

#include "common/cbasetypes.h"

#include "entities/base_entity.h"

class CBattleEntity;
class CCharEntity;
class CMobSpellContainer;

namespace gambits
{

// What gambits need from whoever runs them: a trust's controller today, a player bot's next.
// Spells, weapon skills, abilities and ranged attacks go through the owner's own controller.
class IGambitsHost
{
public:
    virtual ~IGambitsHost() = default;

    // The player whose party the gambits look after.
    virtual auto GambitLeader() const -> CCharEntity* = 0;

    // The spells the owner can pick from.
    virtual auto GambitSpells() const -> CMobSpellContainer* = 0;

    // Who the owner's battle target hates most.
    virtual auto GambitTopEnmity() const -> CBattleEntity* = 0;

    // The owner's place in its party, which staggers members' first actions.
    virtual auto GambitPartyPosition() const -> uint8 = 0;

    // Uses a mob skill (trusts' own skills); hosts without them return false.
    virtual auto GambitMobSkill(EntityId target, uint16 skillId) -> bool = 0;
};

} // namespace gambits
