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

#include "common/cbasetypes.h"
#include "common/mmo.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class CBaseEntity;
class CCharEntity;
class CZone;

// Player bots: server-made player shells that live only in memory, never in the database.
namespace botutils
{

struct BotSpec
{
    std::string                           name;
    uint8                                 race{ 1 };
    uint8                                 face{ 0 };
    uint8                                 size{ 0 };
    uint8                                 mainJob{ 1 };
    uint8                                 mainLevel{ 1 };
    uint8                                 subJob{ 0 };
    uint8                                 subLevel{ 0 };
    position_t                            pos{};
    bool                                  seeking{ false }; // shows the "seeking party" flag while on its own
    std::vector<std::pair<uint8, uint16>> gear;             // equip slot, item id
};

// Bot ids sit above every real character id and below the mob id range (0x01000000).
constexpr uint32 BotIdFirst = 0x00F00000;
constexpr uint32 BotIdLast  = 0x00FFFFFF;

// True for an id in the reserved bot range. The range is checked against the chars table before any
// bot spawns, so a real character never has one.
auto IsBotId(uint32 id) -> bool;

// True when the entity is a player bot.
auto IsBot(const CBaseEntity* PEntity) -> bool;

// Database writes seen with a bot id as a parameter since the map started. Bots must never write, so this
// stays 0; each one is also logged as an error.
auto GetBotDatabaseWrites() -> uint32;

// True when a bot of this job and level can wear the item in that equip slot (checks the gear file).
auto CanWear(uint16 itemId, uint8 job, uint8 level, uint8 equipSlot) -> bool;

// True while a live bot has this name, or a real character has it in the database.
auto IsNameTaken(const std::string& name) -> bool;

// Forgets a despawned bot: frees its name and drops its saved spec.
void Release(uint32 botId, const std::string& name);

// The spec a live bot was built from (for its respawn), or nullptr.
auto GetSpec(uint32 botId) -> const BotSpec*;

// Where players arrive in the zone: the landing points of the zone lines that lead into it.
auto GetEntrances(CZone* PZone) -> const std::vector<position_t>&;

// Puts the position on the zone's floor (its collision mesh), as a player's own client does. Navmesh heights,
// which paths follow, can sit a little above or below the floor; other clients draw a PC at the y it is given.
void SnapToGround(CZone* PZone, position_t& pos);

// True when the bot may pick a fight: the zone has fewer hunting bots than its cap
// (map.PLAYER_BOTS_MAX_HUNTERS, one less for each real player fighting there).
auto MayHunt(CCharEntity* PBot) -> bool;

// Asks the Lua bot manager (xi.bots.populate) whether this zone should gain a bot now.
void Populate(CZone* PZone);

// Builds a bot and puts it in the zone. Returns nullptr when bots are off or the zone cannot take one.
auto SpawnBot(CZone* PZone, const BotSpec& spec) -> CCharEntity*;

// A player invited the bot (packet 0x06E). The bot answers a few seconds later: it joins, or declines and
// says why in a tell (inviter too far above or below its level, party full, trusts out, and so on).
void OnPartyInvite(CCharEntity* PBot, CCharEntity* PInviter);

// Takes the bot out of its party. Real members still here see it go; a party left with only bots ends.
void LeaveParty(CCharEntity* PBot);

// The live bot with this name (case does not matter), or nullptr.
auto FindBotByName(const std::string& name) -> CCharEntity*;

// A player sent the bot a tell; the bot answers with a canned line a moment later.
void OnTell(CCharEntity* PBot, CCharEntity* PSender);

enum class ChatChannel : uint8
{
    Tell,
    Party,
    Say,
};

// Says a canned line for the event (xi.bots.chatLine in Lua picks it, and keeps chat rare); nothing when the
// Lua side has no line or map.PLAYER_BOTS_CHAT is off. A tell needs PTarget.
void Chat(CCharEntity* PBot, const std::string& event, ChatChannel channel, CCharEntity* PTarget = nullptr);

// Now and then a free bot in a city says something (map.PLAYER_BOTS_SAY_INTERVAL per zone).
void IdleChat(CZone* PZone);

} // namespace botutils
