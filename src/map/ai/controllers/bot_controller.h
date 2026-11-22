/*
===========================================================================

  Copyright (c) 2018 Darkstar Dev Teams

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

#include "player_controller.h"

#include "ai/helpers/gambits_host.h"

#include <memory>
#include <unordered_map>

class CCharEntity;
class CMobEntity;
class CMobSpellContainer;

namespace gambits
{

class CGambitsContainer;

}

// Drives a player bot: a solo hunter (roam, hunt, fight to the end, rest) whose fighting uses trust gambits.
// In a party it acts like a trust with the party leader as its master: follow, assist, rest, heal.
// Bots are memory only and custom (not retail); see botutils.
class CBotController : public CPlayerController, public gambits::IGambitsHost
{
public:
    explicit CBotController(CCharEntity* PBot);
    ~CBotController() override;

    DISALLOW_COPY_AND_MOVE(CBotController);

    auto Tick(timer::time_point tick) -> Task<void> override;

    auto GambitLeader() const -> CCharEntity* override;
    auto GambitSpells() const -> CMobSpellContainer* override;
    auto GambitTopEnmity() const -> CBattleEntity* override;
    auto GambitPartyPosition() const -> uint8 override;
    auto GambitMobSkill(EntityId target, uint16 skillId) -> bool override;
    auto GambitShouldTakeHate() const -> bool override;

    auto Gambits() const -> gambits::CGambitsContainer*;

    // Rebuilds the spells and weapon skills the gambits can pick from (after the bot's jobs or level change).
    void RefreshSkills();

    // True while the bot is looking for, or fighting, a mob it picked itself (counts toward the zone's hunter cap).
    auto IsHunting() const -> bool;

    // True once a dead bot has waited long enough for a raise; the zone then moves it to an entrance.
    auto WantsRespawn() const -> bool;

    // True when the bot's play session ran out and it is free to go (alone, not fighting, alive); the zone then
    // logs it out.
    auto WantsLogout() const -> bool;

    // When the bot came into the world (its zone, for a follower).
    auto OnlineSince() const -> timer::time_point;

    // Where the bot roams around and runs back to.
    void SetHome(const position_t& home);

    // A party invite is waiting in the bot's InvitePending; it answers after a short, human-looking delay.
    void QueueInvite();

    // A player invited the bot: true when it will answer, false for a repeat invite within a minute (no answer).
    auto TakeInvite(uint32 inviterId) -> bool;

    // A player sent the bot a tell; it answers after a short delay.
    void QueueTellReply(uint32 senderId);

    // The id of the party leader this bot should follow into another zone now, or 0. The zone moves the bot after
    // its tick (botutils::FollowLeader), since a bot cannot leave its zone from inside it.
    auto FollowTarget() const -> uint32;

    // The bot was just moved into its leader's new zone and put back in the party: carry on as a party member.
    void JoinedByFollow(uint32 leaderId);

private:
    void DeadTick(timer::time_point tick);
    auto CombatTick(timer::time_point tick) -> Task<void>;
    auto SoloTick(timer::time_point tick) -> Task<void>;
    auto PartyTick(timer::time_point tick) -> Task<void>;

    void DecideInvite();
    void AnswerTell();

    // Now and then cures a nearly dead real player close by, or raises a dead one, outside any party.
    void HelpStranger(timer::time_point tick);

    // The real player leading the bot's party in its zone, or nullptr when there is none (zoned, logged out).
    auto PartyLeaderHere() const -> CCharEntity*;

    // The leader is not in the bot's zone: follow it into its new zone, wait while it is on its way, or leave
    // the party when it does not come back (logged out, left the party, or somewhere bots cannot go).
    void LeaderGone(timer::time_point tick);

    // Leaves the party on its own (leader logged out, or cannot be reached), saying goodbye.
    void LeaveParty();

    // Weapon skills: solo as soon as the bot has the TP; in a party, hold TP to close a skillchain.
    void ApplyTpMode();

    // Back on its own after a party: roam and hunt from here.
    void BackToSolo();

    auto FindPrey() -> CMobEntity*;

    // Picks the next stop of the bot's wander across the zone and starts walking there; false when none was found.
    auto StartJourney() -> bool;

    // How long the bot waits at a stop.
    auto StopTime() const -> timer::duration;

    // The nearest mob fighting the bot, or nullptr.
    auto Attacker() const -> CBattleEntity*;

    // True when a real player is this close to the bot.
    auto PlayerWithin(float range) const -> bool;
    auto ShouldRest() const -> bool;
    auto IsResting() const -> bool;
    void StartResting();
    void StopResting();
    auto IsCaster() const -> bool;

    CCharEntity* PBot;

    std::unique_ptr<CMobSpellContainer>         m_spells;
    std::unique_ptr<gambits::CGambitsContainer> m_gambits;

    position_t        m_home{};
    EntityId          m_prey{};
    bool              m_hunting{ false };
    timer::time_point m_nextHuntCheck{};
    timer::time_point m_nextRoam{};
    timer::time_point m_stopUntil{};    // when the bot leaves its current stop
    timer::time_point m_crowdedSince{}; // when a real player came right up to the waiting bot
    timer::time_point m_nextAttackerCheck{};
    bool              m_restingForMP{ false }; // a party bot resting for its own MP, not because the leader rests
    timer::time_point m_deathTime{};
    timer::time_point m_onlineSince{};
    timer::time_point m_logoutAt{}; // when its play session ends (map.PLAYER_BOTS_STAY_MIN to _MAX minutes)

    timer::time_point                             m_inviteAt{};
    uint32                                        m_tellFrom{ 0 };
    std::unordered_map<uint32, timer::time_point> m_lastInvite; // inviter id, when it last invited
    std::unordered_map<uint32, timer::time_point> m_helped;     // stranger id, when the bot last helped them
    timer::time_point                             m_nextGoodDeed{};
    timer::time_point                             m_tellAt{};

    bool   m_inParty{ false };   // was in a party at the last tick (to notice a dismissal)
    uint32 m_partyLeaderId{ 0 }; // the party's leader when last seen here, to follow it or say goodbye

    uint32            m_followLeader{ 0 }; // set during a tick when the bot should follow its leader now
    timer::time_point m_leaderGoneSince{}; // when the leader left the bot's zone
    timer::time_point m_farSince{};        // when the bot fell out of reach of its leader in the same zone

    bool  m_tpForParty{ false }; // the weapon skill mode the gambits run with
    uint8 m_lastLevel{ 0 };      // to rebuild spells and weapon skills when the level changes (level sync)
};
