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
#include <vector>

// Not a retail packet (ours). Answers c2s 0x1F1: whether the GM window's command ran, and the lines the
// command printed to its user (printToPlayer) while it ran, so the window can show them. Those lines
// do not also go to the chat log. Long output is split over several packets with the same RequestId.
//
// Payload: u16 RequestId, u8 Result, u8 Flags (bit 0: last packet; bit 1: output was cut short),
// u8 Count, u8 padding, then Count lines of: u8 Length, Length bytes of text.
class GP_SERV_COMMAND_GM_COMMAND_RESULT final : public GP_SERV_PACKET<PacketS2C::GP_SERV_COMMAND_GM_COMMAND_RESULT, GP_SERV_COMMAND_GM_COMMAND_RESULT>
{
public:
    enum class Result : uint8_t
    {
        Ran     = 0, // The command's onTrigger ran.
        Failed  = 1, // Unknown command, level too low, or a script error.
        Refused = 2, // Not a GM, or in jail.
    };

    static constexpr uint8_t FlagLast      = 0x01;
    static constexpr uint8_t FlagTruncated = 0x02;

    // A packet is at most 508 bytes: 4 header + 6 fixed + lines.
    static constexpr std::size_t MaxLineBytes = 508 - sizeof(GP_SERV_HEADER) - 6;

    struct PacketData
    {
        uint16_t RequestId;
        uint8_t  Result;
        uint8_t  Flags;
        uint8_t  Count;
        uint8_t  padding00;
        uint8_t  Lines[MaxLineBytes];
    };

    // Lines [first, first + count); each line is at most 255 bytes and the caller keeps them within MaxLineBytes.
    GP_SERV_COMMAND_GM_COMMAND_RESULT(uint16_t requestId, Result result, const std::vector<std::string>& lines, std::size_t first, std::size_t count, uint8_t flags);

    // Splits the output into as many packets as it needs (at least one) and pushes them to PChar.
    static void send(class CCharEntity* PChar, uint16_t requestId, Result result, const std::vector<std::string>& lines);
};
