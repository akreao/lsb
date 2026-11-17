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

#include "0x0c5_trust_ascension_equip_set.h"

#include "data/enums/key_item.h"
#include "entities/char_entity.h"
#include "packets/c2s/validation.h"
#include "packets/s2c/0x08f_trust_ascension_equip.h"
#include "trust_ascension.h"

auto GP_CLI_COMMAND_TRUST_ASCENSION_EQUIP_SET::validate(MapSession* PSession, const CCharEntity* PChar) const -> PacketValidationResult
{
    // The client only offers changes in the Mog House (system message 0xB2 elsewhere) with the Celestite Case.
    return PacketValidator(PChar)
        .blockedBy({ BlockedState::InEvent, BlockedState::AbnormalStatus })
        .range("Category", this->Category, 1, trustascension::CelestiteSets)
        .isInMogHouse()
        .hasKeyItem(xi::KeyItem::CelestiteCase);
}

void GP_CLI_COMMAND_TRUST_ASCENSION_EQUIP_SET::process(MapSession* PSession, CCharEntity* PChar) const
{
    // Retail answers each 0x0C5 with one 0x08F. Only accepted changes were seen; on a refused one this sends the
    // unchanged sets back (UNVERIFIED) so the client shows what the server holds.
    trustascension::SetCelestiteSet(PChar, this->Category, this->Slots);
    PChar->pushPacket<GP_SERV_COMMAND_TRUST_ASCENSION_EQUIP>(PChar);
}
