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

#include "base.h"

#include <string>
#include <utility>
#include <vector>

// Not a retail packet (ours). Answers c2s 0x1F0 for modern clients that want to build a GM window.
// It carries the sender's real GM level (m_GMlevel, not the visible badge) and, in pages, the
// commands that level may use: name, permission and the cmdprops parameter string.
// Only ever sent in reply to c2s 0x1F0, so clients that never ask never see this id.
//
// Payload: u8 Version, u8 GmLevel, u16 Total, u16 First, u8 Count, u8 Flags (bit 0: last page),
// then Count entries of: u8 Permission, u8 NameLen, u8 ParamsLen, Name bytes, Params bytes.
class GP_SERV_COMMAND_GM_CATALOG final : public GP_SERV_PACKET<PacketS2C::GP_SERV_COMMAND_GM_CATALOG, GP_SERV_COMMAND_GM_CATALOG>
{
public:
    static constexpr uint8_t  Version      = 1;
    static constexpr uint8_t  FlagLastPage = 0x01;
    // A packet is at most 508 bytes (7-bit size in 4-byte units): 4 header + 8 fixed + entries.
    static constexpr std::size_t MaxEntryBytes = 508 - sizeof(GP_SERV_HEADER) - 8;

    struct Entry
    {
        uint8_t     permission;
        std::string name;
        std::string parameters;

        auto bytes() const -> std::size_t
        {
            return 3 + name.size() + parameters.size();
        }
    };

    struct PacketData
    {
        uint8_t  Version;
        uint8_t  GmLevel;
        uint16_t Total;
        uint16_t First;
        uint8_t  Count;
        uint8_t  Flags;
        uint8_t  Entries[MaxEntryBytes];
    };

    // Entries [first, first + count) of the list; the caller keeps their bytes within MaxEntryBytes.
    GP_SERV_COMMAND_GM_CATALOG(uint8_t gmLevel, const std::vector<Entry>& list, std::size_t first, std::size_t count, bool last);
};
