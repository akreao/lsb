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

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace clientVersion
{

struct Check
{
    bool mismatch = false; // client and server differ in year + month
    bool fatal    = false; // the lobby refuses the client (error 331)
};

// Versions look like "30261102_0". Only year + month (the first 6 characters)
// are compared; the rest is masked as "xx_x", as the client sends a language
// letter there.
inline auto masked(std::string_view version) -> std::string
{
    return std::string(version.substr(0, 6)) + "xx_x";
}

// verLock: 0 = off, 1 = exact match only, 2 = expected version or newer.
inline auto check(std::string_view expected, std::string_view client, uint8_t verLock) -> Check
{
    const auto want = masked(expected);
    const auto got  = masked(client);

    Check result{};
    result.mismatch = want != got;
    if (!result.mismatch)
    {
        return result;
    }

    switch (verLock)
    {
        case 1:
            result.fatal = true;
            break;
        case 2:
            result.fatal = want > got;
            break;
        default:
            break;
    }

    return result;
}

} // namespace clientVersion
