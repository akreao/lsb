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

class CCharEntity;
class CMobEntity;
class CMobSpellContainer;

namespace gambits
{

class CGambitsContainer;

}

// Drives a player bot: a solo hunter (roam, hunt, fight, rest, flee) whose fighting uses trust gambits.
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

    auto Gambits() const -> gambits::CGambitsContainer*;

    // Rebuilds the spells and weapon skills the gambits can pick from (after the bot's jobs or level change).
    void RefreshSkills();

    // True while the bot is looking for, or fighting, a mob it picked itself (counts toward the zone's hunter cap).
    auto IsHunting() const -> bool;

    // True once a dead bot has waited long enough for a raise; the zone then moves it to an entrance.
    auto WantsRespawn() const -> bool;

    // Where the bot roams around and runs back to.
    void SetHome(const position_t& home);

    // A party invite is waiting in the bot's InvitePending; it answers after a short, human-looking delay.
    void QueueInvite();

    // A player sent the bot a tell; it answers after a short delay.
    void QueueTellReply(uint32 senderId);

private:
    void DeadTick(timer::time_point tick);
    auto CombatTick(timer::time_point tick) -> Task<void>;
    auto SoloTick(timer::time_point tick) -> Task<void>;
    auto PartyTick(timer::time_point tick) -> Task<void>;

    void DecideInvite();
    void AnswerTell();

    // The real player leading the bot's party in its zone, or nullptr when there is none (zoned, logged out).
    auto PartyLeaderHere() const -> CCharEntity*;

    // Leaves the party on its own (leader zoned or logged out), saying goodbye.
    void LeaveParty();

    // Back on its own after a party: roam and hunt from here.
    void BackToSolo();

    auto FindPrey() -> CMobEntity*;
    auto ShouldRest() const -> bool;
    auto IsResting() const -> bool;
    void StartResting();
    void StopResting();
    auto ShouldFlee(const CBattleEntity* PTarget) const -> bool;
    auto IsCaster() const -> bool;

    CCharEntity* PBot;

    std::unique_ptr<CMobSpellContainer>         m_spells;
    std::unique_ptr<gambits::CGambitsContainer> m_gambits;

    position_t        m_home{};
    EntityId          m_prey{};
    bool              m_hunting{ false };
    bool              m_fleeing{ false };
    timer::time_point m_nextHuntCheck{};
    timer::time_point m_nextRoam{};
    timer::time_point m_deathTime{};

    timer::time_point m_inviteAt{};
    uint32            m_tellFrom{ 0 };
    timer::time_point m_tellAt{};

    bool   m_inParty{ false };   // was in a party at the last tick (to notice a dismissal)
    uint32 m_partyLeaderId{ 0 }; // who invited it, to say goodbye when dismissed
};
