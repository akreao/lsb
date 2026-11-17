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

#include "0x0c6_trust_ascension_equip_req.h"

#include "entities/char_entity.h"
#include "packets/c2s/validation.h"
#include "packets/s2c/0x08f_trust_ascension_equip.h"

auto GP_CLI_COMMAND_TRUST_ASCENSION_EQUIP_REQ::validate(MapSession* PSession, const CCharEntity* PChar) const -> PacketValidationResult
{
    return PacketValidator(PChar);
}

void GP_CLI_COMMAND_TRUST_ASCENSION_EQUIP_REQ::process(MapSession* PSession, CCharEntity* PChar) const
{
    // Never seen from retail (0x08F always came first after zoning); answered with the same block.
    PChar->pushPacket<GP_SERV_COMMAND_TRUST_ASCENSION_EQUIP>(PChar);
}
