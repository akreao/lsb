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

#include "0x1f1_gm_command_result.h"

#include "entities/char_entity.h"

#include <algorithm>
#include <cstring>

namespace
{

// Output kept per command: about 16 full packets. The rest is dropped and flagged.
constexpr std::size_t MaxPackets = 16;

auto lineBytes(const std::string& line) -> std::size_t
{
    return 1 + std::min<std::size_t>(line.size(), 255);
}

} // namespace

GP_SERV_COMMAND_GM_COMMAND_RESULT::GP_SERV_COMMAND_GM_COMMAND_RESULT(const uint16_t requestId, const Result result, const std::vector<std::string>& lines, const std::size_t first, const std::size_t count, const uint8_t flags)
{
    auto& packet = this->data();

    packet.RequestId = requestId;
    packet.Result    = static_cast<uint8_t>(result);
    packet.Flags     = flags;

    std::size_t offset  = 0;
    uint8_t     written = 0;
    for (std::size_t i = first; i < first + count && i < lines.size(); ++i)
    {
        const auto length = std::min<std::size_t>(lines[i].size(), 255);
        if (offset + 1 + length > MaxLineBytes)
        {
            break;
        }

        packet.Lines[offset++] = static_cast<uint8_t>(length);
        std::memcpy(&packet.Lines[offset], lines[i].data(), length);
        offset += length;
        ++written;
    }

    packet.Count = written;
    this->setSize(sizeof(GP_SERV_HEADER) + 6 + offset);
}

void GP_SERV_COMMAND_GM_COMMAND_RESULT::send(CCharEntity* const PChar, const uint16_t requestId, const Result result, const std::vector<std::string>& lines)
{
    std::size_t first   = 0;
    std::size_t packets = 0;
    do
    {
        std::size_t bytes = 0;
        std::size_t count = 0;
        while (first + count < lines.size() && count < 255 && bytes + lineBytes(lines[first + count]) <= MaxLineBytes)
        {
            bytes += lineBytes(lines[first + count]);
            ++count;
        }

        ++packets;
        const bool more      = first + count < lines.size();
        const bool truncated = more && packets == MaxPackets;
        uint8_t    flags     = (!more || truncated) ? FlagLast : 0;
        if (truncated || std::any_of(lines.begin() + first, lines.begin() + first + count, [](const auto& l) { return l.size() > 255; }))
        {
            flags |= FlagTruncated;
        }

        PChar->pushPacket<GP_SERV_COMMAND_GM_COMMAND_RESULT>(requestId, result, lines, first, count, flags);
        first += count;

        if (truncated)
        {
            break;
        }
    }
    while (first < lines.size());
}
