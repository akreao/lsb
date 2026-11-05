/*
===========================================================================

  Copyright (c) 2010-2015 Darkstar Dev Teams

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

#include <common/cbasetypes.h>
#include <common/scheduler/scheduler.h>

#include <string>

//
// Forward declarations
//

class CCharEntity;

namespace sol
{

class state;

}

enum class CommandResult : uint8
{
    Success,
    Failure,
};

class CCommandHandler
{
public:
    static auto call(Scheduler& scheduler, sol::state& lua, CCharEntity* PChar, const std::string& commandline) -> CommandResult;

    // Ours: sends the character's real GM level and the commands it may use (s2c 0x1F0 pages).
    static void sendCatalog(sol::state& lua, CCharEntity* PChar);

    // Ours: re-sends the catalogue after a GM level change, if the client asked for one before.
    static void onGmLevelChanged(sol::state& lua, CCharEntity* PChar);
};
