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

#include "0x08e_trust_ascension_points.h"

#include "entities/char_entity.h"
#include "enums/alter_ego_points.h"
#include "trust_ascension.h"
#include "utils/charutils.h"

GP_SERV_COMMAND_TRUST_ASCENSION_POINTS::GP_SERV_COMMAND_TRUST_ASCENSION_POINTS(CCharEntity* PChar)
{
    auto& packet = this->data();

    packet.Points = charutils::GetPoints(PChar, "alter_ego_points");

    // Retail fills the levels and the next level's cost of the attribute ids 8..18 only, with or without
    // the Cipher Bracelet; every other entry stays 0.
    for (uint8 category = static_cast<uint8>(AlterEgoCategory::HP); category <= static_cast<uint8>(AlterEgoCategory::MagicSkills); category++)
    {
        packet.Upgrades[category] = PChar->m_alterEgo[category];
        packet.Costs[category]    = trustascension::AlterEgoCost(PChar->m_alterEgo[category]);
    }
}
