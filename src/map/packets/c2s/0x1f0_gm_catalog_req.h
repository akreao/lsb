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

// Not a retail packet (ours). A modern client asks for its real GM level and the GM commands it may use.
// The reply is one or more s2c 0x1F0 pages. Retail clients never send it.
GP_CLI_PACKET(GP_CLI_COMMAND_GM_CATALOG_REQ,
              uint8_t Version;    // Highest catalogue version the client reads (1).
              uint8_t padding00;  // Padding; unused.
              uint16_t padding01; // Padding; unused.
);
