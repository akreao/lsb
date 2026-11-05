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

#include "0x1f0_gm_catalog.h"

#include <cstring>

GP_SERV_COMMAND_GM_CATALOG::GP_SERV_COMMAND_GM_CATALOG(const uint8_t gmLevel, const std::vector<Entry>& list, const std::size_t first, const std::size_t count, const bool last)
{
    auto& packet = this->data();

    packet.Version = Version;
    packet.GmLevel = gmLevel;
    packet.Total   = static_cast<uint16_t>(list.size());
    packet.First   = static_cast<uint16_t>(first);
    packet.Count   = static_cast<uint8_t>(count);
    packet.Flags   = last ? FlagLastPage : 0;

    std::size_t offset = 0;
    for (std::size_t i = first; i < first + count && i < list.size(); ++i)
    {
        const auto& entry = list[i];
        if (offset + entry.bytes() > MaxEntryBytes)
        {
            break;
        }

        packet.Entries[offset++] = entry.permission;
        packet.Entries[offset++] = static_cast<uint8_t>(entry.name.size());
        packet.Entries[offset++] = static_cast<uint8_t>(entry.parameters.size());
        std::memcpy(&packet.Entries[offset], entry.name.data(), entry.name.size());
        offset += entry.name.size();
        std::memcpy(&packet.Entries[offset], entry.parameters.data(), entry.parameters.size());
        offset += entry.parameters.size();
    }

    this->setSize(sizeof(GP_SERV_HEADER) + 8 + offset);
}
