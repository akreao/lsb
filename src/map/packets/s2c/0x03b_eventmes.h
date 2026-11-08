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

class CBaseEntity;

// https://github.com/atom0s/XiPackets/tree/main/world/server/0x003B
// This packet is sent by the server to display a formatted message loaded from the DAT files.
// Unlike 0x036, the retail client shows the line in the event message window (paged, waits for Enter when the
// text asks for it) even when no event is running. The client drops it unless the entity is loaded with this id and
// index. It clears these lines when fishing ends, an event ends or the zone changes.
// Number bit 15 set: no "Name : " prefix (same as 0x036).
class GP_SERV_COMMAND_EVENTMES final : public GP_SERV_PACKET<PacketS2C::GP_SERV_COMMAND_EVENTMES, GP_SERV_COMMAND_EVENTMES>
{
public:
    struct PacketData
    {
        uint32_t UniqueNo; // PS2: UniqueNo
        uint16_t ActIndex; // PS2: ActIndex
        uint16_t Number;   // PS2: Number
    };

    GP_SERV_COMMAND_EVENTMES(const CBaseEntity* PEntity, uint16 messageID, bool showName = true);
};
