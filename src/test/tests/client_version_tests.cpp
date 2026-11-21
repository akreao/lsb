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

#include "login/client_version.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("client version masks everything after year and month", "[client_version]")
{
    CHECK(clientVersion::masked("30261102_0") == "302611xx_x");
    CHECK(clientVersion::masked("302611") == "302611xx_x");

    // The day, build and language letter never count as a mismatch.
    const auto sameMonth = clientVersion::check("30261102_0", "30261130_1", 1);
    CHECK_FALSE(sameMonth.mismatch);
    CHECK_FALSE(sameMonth.fatal);
}

TEST_CASE("client version lock rules", "[client_version]")
{
    const auto older = "30261006_0";
    const auto match = "30261102_0";
    const auto newer = "30261201_0";

    // 0: never refuses, still records the mismatch.
    CHECK(clientVersion::check(match, older, 0).mismatch);
    CHECK_FALSE(clientVersion::check(match, older, 0).fatal);

    // 1: exact year + month only.
    CHECK(clientVersion::check(match, older, 1).fatal);
    CHECK(clientVersion::check(match, newer, 1).fatal);
    CHECK_FALSE(clientVersion::check(match, match, 1).fatal);

    // 2: expected version or newer.
    CHECK(clientVersion::check(match, older, 2).fatal);
    CHECK(clientVersion::check(match, newer, 2).mismatch);
    CHECK_FALSE(clientVersion::check(match, newer, 2).fatal);
    CHECK_FALSE(clientVersion::check(match, match, 2).mismatch);
}
