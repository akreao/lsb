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

#include "0x1f0_gm_catalog_req.h"

#include "command_handler.h"
#include "common/timer.h"
#include "common/lua.h"
#include "entities/char_entity.h"
#include "packets/c2s/validation.h"

auto GP_CLI_COMMAND_GM_CATALOG_REQ::validate(MapSession* PSession, const CCharEntity* PChar) const -> PacketValidationResult
{
    return PacketValidator(PChar)
        .mustNotEqual(this->Version, 0, "Catalogue version 0");
}

void GP_CLI_COMMAND_GM_CATALOG_REQ::process(MapSession* PSession, CCharEntity* PChar) const
{
    // Once asked, the server also pushes a fresh catalogue when the GM level changes.
    PChar->m_wantsGmCatalog = true;

    // A catalogue is several packets, so answer at most one request every 5 seconds.
    const auto now = timer::now();
    if (PChar->m_gmCatalogSentAt != timer::time_point{} && now < PChar->m_gmCatalogSentAt + 5s)
    {
        return;
    }

    PChar->m_gmCatalogSentAt = now;
    CCommandHandler::sendCatalog(lua, PChar);
}
