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

#include "blacklistutils.h"

#include "common/utils.h"

#include "entities/char_entity.h"

#include "packets/s2c/0x041_black_list.h"
#include "utils/botutils.h"

#include <algorithm>
#include <iterator>

namespace blacklistutils
{

auto IsBlacklisted(uint32 ownerId, uint32 targetId) -> bool
{
    const auto rset = db::preparedStmt("SELECT * FROM char_blacklist WHERE charid_owner = ? AND charid_target = ? LIMIT 1", ownerId, targetId);
    return rset && rset->rowsCount();
}

auto AddBlacklisted(uint32 ownerId, uint32 targetId) -> bool
{
    if (IsBlacklisted(ownerId, targetId))
    {
        return false;
    }

    const auto rset = db::preparedStmt("INSERT INTO char_blacklist (charid_owner, charid_target) VALUES (?, ?)", ownerId, targetId);
    return rset && rset->rowsAffected();
}

auto DeleteBlacklisted(uint32 ownerId, uint32 targetId) -> bool
{
    if (!IsBlacklisted(ownerId, targetId))
    {
        return false;
    }

    const auto rset = db::preparedStmt("DELETE FROM char_blacklist WHERE charid_owner = ? AND charid_target = ? LIMIT 1", ownerId, targetId);
    return rset && rset->rowsAffected();
}

void SendBlacklist(CCharEntity* PChar)
{
    auto isNameCharactersOnly = [](const std::string& name) -> bool
    {
        // null terminator added for paranoia, the docs say `find_first_not_of` _will_ check those.
        // https://en.cppreference.com/w/cpp/string/basic_string/find_first_not_of
        return to_lower(name).find_first_not_of("abcdefghijklmnopqrstuvwxyz\0") == std::string::npos;
    };

    // Obtain this users blacklist info
    std::vector<std::pair<uint32, std::string>> entries;

    const auto rset = db::preparedStmt("SELECT c.charid, c.charname FROM char_blacklist AS b INNER JOIN chars AS c ON b.charid_target = c.charid WHERE charid_owner = ?", PChar->id);
    while (rset && rset->next())
    {
        auto accid_target = rset->get<uint32>(0);
        auto targetName   = rset->get<std::string>(1);

        if (isNameCharactersOnly(targetName))
        {
            entries.emplace_back(accid_target, targetName);
        }
    }

    // Player bots the player blacklisted this session (memory only).
    std::ranges::copy(botutils::BlacklistedBots(PChar), std::back_inserter(entries));

    if (entries.empty())
    {
        PChar->pushPacket<GP_SERV_COMMAND_BLACK_LIST>(
            entries,
            GP_SERV_COMMAND_BLACK_LIST::ResetClientBlacklist::Yes,
            GP_SERV_COMMAND_BLACK_LIST::LastBlacklistPacket::Yes);

        return;
    }

    // At most 12 entries per packet; the first resets the client's list, the last says so.
    for (std::size_t start = 0; start < entries.size(); start += 12)
    {
        const auto end = std::min(start + 12, entries.size());

        PChar->pushPacket<GP_SERV_COMMAND_BLACK_LIST>(
            std::vector<std::pair<uint32, std::string>>(entries.begin() + start, entries.begin() + end),
            GP_SERV_COMMAND_BLACK_LIST::ResetClientBlacklist{ start == 0 },
            GP_SERV_COMMAND_BLACK_LIST::LastBlacklistPacket{ end == entries.size() });
    }
}

} // namespace blacklistutils
