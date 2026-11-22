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
class CMobEntity;
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

// Takes a bot out of its zone now (out of its party first). Not for use inside that zone's tick.
void Despawn(CCharEntity* PBot);

// Forgets every bot-wide memory (names, specs, entrances, chat timers). Tests call it between cases.
void ResetForTests();

// Keeps the saved spec in step with the bot (its level after level-ups), so a respawn brings back the same bot.
void UpdateSpec(const CCharEntity* PBot);

// Counts rows with a bot id in every char* table that has a charid column. Bots must never be saved, so this is
// 0; otherwise it logs an error and tells the online GMs. Returns the count.
auto CheckDatabaseForBots() -> uint32;

// Runs CheckDatabaseForBots once a day while bots are on (called from the zone tick; cheap when not due).
void DailyDatabaseCheck(timer::time_point tick);

// Rewrites player_bots_search (the /sea list of bots) with every live bot, or none while bots are off. Returns the
// rows written.
auto RefreshSearchList() -> uint32;

// Calls RefreshSearchList every 10 seconds (called from every zone tick; cheap when not due).
void SearchListTick(timer::time_point tick);

// Forgets a despawned bot: frees its name and drops its saved spec.
void Release(uint32 botId, const std::string& name);

// The spec a live bot was built from (for its respawn), or nullptr.
auto GetSpec(uint32 botId) -> const BotSpec*;

// Where players arrive in the zone: the landing points of the zone lines that lead into it.
auto GetEntrances(CZone* PZone) -> const std::vector<position_t>&;

// The height of the first floor (collision mesh) a ray from top down to bottom meets at the position's x and z,
// or nothing. Down is +y, so top < bottom.
auto GroundBelow(CZone* PZone, const position_t& pos, float top, float bottom) -> Maybe<float>;

// Puts a bot's position on the floor a player could stand on there: the navmesh floor straight over or under it
// nearest its height (a point off the navmesh slides onto a floor close by), then the collision face nearest that.
void StandOnGround(CZone* PZone, position_t& pos);

// True when the bot may pick a fight: hunting is allowed in the zone (not a starting zone on
// map.PLAYER_BOTS_NO_HUNT_ZONES, fewer than 3 real players of the zone's level there), and the zone has fewer
// hunting bots than its cap (map.PLAYER_BOTS_MAX_HUNTERS, one less per real player of the zone's level).
auto MayHunt(CCharEntity* PBot) -> bool;

// True when the mob script for this zone and mob name makes the mob an NM's placeholder (it calls
// xi.mob.phOnDespawn). Bots never hunt placeholders.
auto IsPlaceholderScript(const std::string& zoneName, const std::string& mobName) -> bool;

// The real players a zone counts: not GM-hidden, not leaving.
auto RealPlayers(CZone* PZone) -> std::vector<CCharEntity*>;

// True when a bot of this level may party with a party whose highest real member has the given level
// (map.PLAYER_BOTS_PARTY_LEVEL_ABOVE and _BELOW).
auto FitsPartyLevel(uint8 highest, uint8 level) -> bool;

// A real player in the zone, other than the inviter, who shows the "seeking party" flag, has no party and fits
// a party of this top level; nullptr when there is none. Bots turn invites down while one waits: people first.
auto FindSeeker(CZone* PZone, const CCharEntity* PInviter, uint8 highest) -> CCharEntity*;

// Notes where real players are fighting; a camp stays for map.PLAYER_BOTS_CAMP_MINUTES after their last fight.
// Called every zone tick while the zone has real players.
void TrackCamps(CZone* PZone);

// True inside a real player's camp (map.PLAYER_BOTS_CAMP_RANGE yalms): bots never hunt there.
auto InPlayerCamp(CZone* PZone, const position_t& pos) -> bool;

// True when a bot may go after this mob: a normal, respawning mob that is no NM's placeholder, has no claim or
// fight, and no real player is near it or has it targeted.
auto IsFairPrey(CCharEntity* PBot, CMobEntity* PMob) -> bool;

// Lets go of every claim and hate the bot holds (when it dies), so the mob is free for anyone.
void DropClaims(CCharEntity* PBot);

// Asks the Lua bot manager (xi.bots.populate) whether this zone should gain a bot now.
void Populate(CZone* PZone);

// Builds a bot and puts it in the zone. Returns nullptr when bots are off or the zone cannot take one.
auto SpawnBot(CZone* PZone, const BotSpec& spec) -> CCharEntity*;

// A player invited the bot (packet 0x06E). The bot answers a few seconds later: it joins, or declines and
// says why in a tell (inviter too far above or below its level, party full, trusts out, and so on).
void OnPartyInvite(CCharEntity* PBot, CCharEntity* PInviter);

// Takes the bot out of its party. Real members still here see it go; a party left with only bots ends.
void LeaveParty(CCharEntity* PBot);

// True when a party bot may follow this leader where it is now: bots are on, the zone takes bots (not an
// instance, Dynamis or an excluded zone) and the leader is not in a Mog House.
auto MayFollowInto(const CCharEntity* PLeader) -> bool;

// Moves a party bot to its leader's zone (custom): the bot leaves its zone and a copy from the same spec, at the
// level it reached, arrives next to the leader and takes its seat in the same party. Called by the bot's zone
// after its tick; PBot is freed.
void FollowLeader(CCharEntity* PBot, CCharEntity* PLeader);

// The live bot with this name (case does not matter), or nullptr.
auto FindBotByName(const std::string& name) -> CCharEntity*;

// The live bot with this id anywhere on the map process, or nullptr.
auto FindBotById(uint32 id) -> CCharEntity*;

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

// The char var a real player sets with `!bots off` (or the website): bots are not drawn for them, never talk to
// them and never hunt near them (custom).
constexpr auto OptOutVar = "BotsOptOut";

// True when the player wants nothing from this bot: bots turned off, or the bot is on their blacklist.
auto IgnoresBot(const CCharEntity* PChar, const CCharEntity* PBot) -> bool;

// /blacklist for bots, which have no database row: kept in memory until the player logs out. Add returns the id
// the client gets for the entry, 0 when there is no live bot of that name, it is listed already or the list is
// full; remove returns the entry's id, or 0 when it was not listed.
auto BlacklistBot(CCharEntity* PChar, const std::string& name) -> uint32;
auto UnblacklistBot(const CCharEntity* PChar, const std::string& name) -> uint32;

// The bots on the player's blacklist (id, name), for the full list sent at zone-in.
auto BlacklistedBots(const CCharEntity* PChar) -> std::vector<std::pair<uint32, std::string>>;

// The player logged out: forget their bot blacklist.
void ForgetPlayer(uint32 charId);

// True when the real player turned bots off (`!bots off`): bots are not drawn for them (SpawnPCs).
auto OptedOut(const CCharEntity* PChar) -> bool;

// Bots alive on this map process (map.PLAYER_BOTS_MAX_TOTAL caps it).
auto GetLiveBotCount() -> std::size_t;

// A random point on the zone's navmesh, reachable from an entrance and out of every real player's sight: where
// the first bots of a newly visited zone already are, as if they had been out there all along.
auto ScatterPoint(CZone* PZone) -> Maybe<position_t>;

// The bot logs out on its own (its play session ran out): the Lua side may keep it as a regular who comes back.
void OnLogout(CCharEntity* PBot);

// What a live bot is doing, for GM tools (`!bot whois`, `!bot all`) and later the admin site.
struct BotInfo
{
    uint32      id{ 0 };
    std::string name;
    uint16      zoneId{ 0 };
    std::string zoneName;
    uint8       mainJob{ 0 };
    uint8       mainLevel{ 0 };
    uint8       subJob{ 0 };
    uint8       subLevel{ 0 };
    std::string state;     // "dead", "fighting", "hunting", "party", "resting", "seeking", "roaming"
    std::string partyWith; // the party leader's name, when in a party
    position_t  pos{};
    uint32      onlineSeconds{ 0 };
};

// Every live bot on this map process.
auto ListBots() -> std::vector<BotInfo>;

// `!bot pause`: no new bots arrive anywhere and no bot starts a hunt (fights under way finish; parties go on).
void SetPaused(bool paused);
auto IsPaused() -> bool;

} // namespace botutils
