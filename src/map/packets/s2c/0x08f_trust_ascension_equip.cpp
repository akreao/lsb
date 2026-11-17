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

#include "0x08f_trust_ascension_equip.h"

#include "entities/char_entity.h"

#include <cstring>

GP_SERV_COMMAND_TRUST_ASCENSION_EQUIP::GP_SERV_COMMAND_TRUST_ASCENSION_EQUIP(const CCharEntity* PChar)
{
    auto& packet = this->data();

    static_assert(sizeof(packet.Owned) == sizeof(PChar->m_celestite.owned));
    static_assert(sizeof(packet.Categories) == sizeof(PChar->m_celestite.sets));

    std::memcpy(packet.Owned, PChar->m_celestite.owned, sizeof(packet.Owned));
    std::memcpy(packet.Categories, PChar->m_celestite.sets, sizeof(packet.Categories));
}
