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

#include "0x0c1_trust_ascension_points.h"

#include "data/enums/key_item.h"
#include "entities/char_entity.h"
#include "enums/alter_ego_points.h"
#include "packets/c2s/validation.h"
#include "trust_ascension.h"

auto GP_CLI_COMMAND_TRUST_ASCENSION_POINTS::validate(MapSession* PSession, const CCharEntity* PChar) const
    -> PacketValidationResult
{
    return PacketValidator(PChar)
        .blockedBy({ BlockedState::InEvent, BlockedState::AbnormalStatus })
        .oneOf<AlterEgoCategory>(this->CategoryIndex)
        .isInMogHouse()
        .hasKeyItem(xi::KeyItem::CipherBracelet)
        .custom([&](PacketValidator& v)
                {
                    if (PChar->GetMLevel() < 99)
                    {
                        v.mustEqual(true, false, "Main job must be Level 99");
                    }
                });
}

void GP_CLI_COMMAND_TRUST_ASCENSION_POINTS::process(MapSession* PSession, CCharEntity* PChar) const
{
    // Retail was only seen accepting upgrades; its answer to one the points or the cap refuse is not known.
    trustascension::UpgradeAlterEgo(PChar, static_cast<uint8>(this->CategoryIndex));
}
