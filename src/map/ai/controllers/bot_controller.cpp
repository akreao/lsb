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

#include "bot_controller.h"

#include "ai/ai_container.h"
#include "ai/helpers/gambits_container.h"
#include "ai/helpers/pathfind.h"
#include "ai/states/ability_state.h"
#include "ai/states/magic_state.h"
#include "ai/states/range_state.h"
#include "ai/states/weaponskill_state.h"
#include "common/settings.h"
#include "common/utils.h"
#include "enmity_container.h"
#include "entities/char_entity.h"
#include "entities/mob_entity.h"
#include "enums/msg_std.h"
#include "mob_spell_container.h"
#include "navmesh/navmesh.h"
#include "packets/s2c/0x009_message.h"
#include "party.h"
#include "spell.h"
#include "status_effect_container.h"
#include "utils/battleutils.h"
#include "utils/botutils.h"
#include "utils/charutils.h"
#include "utils/zoneutils.h"
#include "weapon_skill.h"
#include "zone.h"

#include <limits>

namespace
{

// How far a solo bot looks for a mob, and how close a real player may be before it leaves that mob alone.
constexpr float PreySearchRange = 40.0f;
constexpr float EngageRange     = 20.0f;
constexpr float CasterRange     = 10.0f;

// A solo bot wanders the zone in legs: it runs to a stop this far away anywhere in the zone, waits there a while,
// then picks the next one. It never seeks players out, and a player who stays next to a waiting bot sees it move
// on, so bots pass people by chance rather than gather around them (custom).
constexpr float JourneyMin       = 60.0f;
constexpr float JourneyMax       = 150.0f;
constexpr float CrowdRange       = 15.0f;
constexpr auto  CrowdPatience    = 30s;
constexpr float StopWanderRadius = 6.0f;

// Near a real player a bot looks for prey only one check in this many (it still never takes a player's mob).
constexpr float NearPlayerRange    = 40.0f;
constexpr int   NearPlayerHuntOdds = 3;

// In a party: how close the bot keeps to its leader, and how far it runs to join a fight.
constexpr float FollowDistance = 3.0f;
constexpr float FollowSlack    = 2.0f;

// A party bot below this much MP rests on its own between fights.
constexpr uint8 PartyRestMPP = 30;
constexpr float AssistRange  = 30.0f;

// A bot that stays farther than AssistRange from its leader this long (no way through) leaves the party.
constexpr auto ReachTimeout = 60s;

// How long a party bot waits for a leader who left its zone to show up somewhere (zoning, a Mog House visit).
constexpr auto LeaderWait = 2min;

// A zoned-in leader has no party until its party reloads from the database; this long without one means it left.
constexpr auto PartyReloadWait = 10s;

// A solo bot now and then cures a nearly dead stranger or raises a dead one close by (custom). It checks this
// often, acts on one check in GoodDeedOdds, and leaves the same player be for a while after helping.
constexpr auto  GoodDeedCheck      = 5s;
constexpr int   GoodDeedOdds       = 2;
constexpr float GoodDeedRange      = 20.0f;
constexpr uint8 GoodDeedHPP        = 30;
constexpr auto  GoodDeedRepeatWait = 2min;

// More invites from the same player within this time get no answer.
constexpr auto InviteRepeatWait = 60s;

// Main jobs a party bot leaves the tanking to, and the ones it protects when it does tank.
auto isTankJob(const xi::Job job) -> bool
{
    return job == xi::Job::PLD || job == xi::Job::NIN || job == xi::Job::RUN;
}

auto isHealerOrCaster(const xi::Job job) -> bool
{
    switch (job)
    {
        case xi::Job::WHM:
        case xi::Job::BLM:
        case xi::Job::RDM:
        case xi::Job::BRD:
        case xi::Job::SMN:
        case xi::Job::SCH:
        case xi::Job::GEO:
            return true;
        default:
            return false;
    }
}

auto isBusy(const CCharEntity* PBot) -> bool
{
    return PBot->PAI->IsCurrentState<CMagicState>() || PBot->PAI->IsCurrentState<CAbilityState>() ||
           PBot->PAI->IsCurrentState<CWeaponSkillState>() || PBot->PAI->IsCurrentState<CRangeState>();
}

} // namespace

CBotController::CBotController(CCharEntity* PBot)
: CPlayerController(PBot)
, PBot(PBot)
, m_spells(std::make_unique<CMobSpellContainer>(PBot))
, m_gambits(std::make_unique<gambits::CGambitsContainer>(PBot, this))
, m_home(PBot->loc.p)
, m_stopUntil(timer::now() + std::chrono::seconds(xirand::GetRandomNumber(5, 20)))
, m_lastLevel(PBot->GetMLevel())
{
    // Bots play for a while and log out, as people do (custom); new ones come in their place.
    const auto stayMin = settings::get<uint16>("map.PLAYER_BOTS_STAY_MIN");
    const auto stayMax = std::max(stayMin, settings::get<uint16>("map.PLAYER_BOTS_STAY_MAX"));
    m_onlineSince      = timer::now();
    m_logoutAt         = m_onlineSince + std::chrono::minutes(xirand::GetRandomNumber<int32>(stayMin, stayMax + 1));

    RefreshSkills();
}

CBotController::~CBotController() = default;

auto CBotController::GambitLeader() const -> CCharEntity*
{
    // In a party the gambits look after the leader's party (the bot is in it); solo, after the bot alone.
    if (auto* PLeader = PartyLeaderHere())
    {
        return PLeader;
    }

    return PBot;
}

auto CBotController::GambitSpells() const -> CMobSpellContainer*
{
    return m_spells.get();
}

auto CBotController::GambitTopEnmity() const -> CBattleEntity*
{
    if (const auto* PMob = dynamic_cast<CMobEntity*>(PBot->GetBattleTarget()))
    {
        return PMob->PEnmityContainer->GetHighestEnmity();
    }

    return nullptr;
}

auto CBotController::GambitPartyPosition() const -> uint8
{
    if (PBot->PParty == nullptr)
    {
        return 0;
    }

    const auto& members = PBot->PParty->members;
    const auto  it      = std::ranges::find(members, PBot);
    return it != members.end() ? static_cast<uint8>(std::distance(members.begin(), it)) : 0;
}

auto CBotController::GambitMobSkill(EntityId /*target*/, uint16 /*skillId*/) -> bool
{
    return false;
}

auto CBotController::GambitShouldTakeHate() const -> bool
{
    const auto* PTop = GambitTopEnmity();
    if (PTop == nullptr || PTop == PBot)
    {
        return false;
    }

    // Solo the bot is the only one there to hold hate.
    if (PBot->PParty == nullptr)
    {
        return true;
    }

    // In a party a real PLD, NIN or RUN does the tanking; the bot never takes hate off them.
    for (const auto* PMember : PBot->PParty->members)
    {
        if (PMember->objtype == TYPE_PC && !botutils::IsBot(PMember) && isTankJob(PMember->GetMJob()))
        {
            return false;
        }
    }

    // Otherwise it steps in only for someone who cannot take the hits.
    const auto* PMob = dynamic_cast<CMobEntity*>(PBot->GetBattleTarget());
    if (PMob == nullptr)
    {
        return false;
    }

    const auto* PVictim = PMob->GetBattleTarget();
    return PVictim != nullptr && PVictim != PBot && isHealerOrCaster(PVictim->GetMJob());
}

auto CBotController::Gambits() const -> gambits::CGambitsContainer*
{
    return m_gambits.get();
}

void CBotController::RefreshSkills()
{
    m_spells->ClearSpells();
    for (uint16 spellId = 1; spellId < MAX_SPELL_ID; ++spellId)
    {
        if (charutils::hasSpell(PBot, spellId) == 0)
        {
            continue;
        }

        auto* PSpell = spell::GetSpell(static_cast<SpellID>(spellId));
        if (PSpell == nullptr)
        {
            continue;
        }

        const bool mainCan = PSpell->getJob(PBot->GetMJob()) <= PBot->GetMLevel();
        const bool subCan  = PBot->GetSJob() != xi::Job::NONE && PSpell->getJob(PBot->GetSJob()) <= PBot->GetSLevel();
        if (mainCan || subCan)
        {
            m_spells->AddSpell(static_cast<SpellID>(spellId));
        }
    }

    m_gambits->tp_skills.clear();
    m_gambits->tp_select = gambits::G_SELECT::RANDOM;
    ApplyTpMode();

    for (uint16 wsId = 1; wsId < 256; ++wsId)
    {
        if (charutils::hasWeaponSkill(PBot, wsId) == 0)
        {
            continue;
        }

        if (const auto* PWeaponSkill = battleutils::GetWeaponSkill(wsId))
        {
            m_gambits->tp_skills.emplace_back(gambits::G_REACTION::WS,
                                              wsId,
                                              PWeaponSkill->getPrimarySkillchain(),
                                              PWeaponSkill->getSecondarySkillchain(),
                                              PWeaponSkill->getTertiarySkillchain(),
                                              battleutils::isValidSelfTargetWeaponskill(wsId) ? TARGET_SELF : TARGET_ENEMY);
        }
    }
}

auto CBotController::IsHunting() const -> bool
{
    return m_hunting && PBot->isAlive();
}

auto CBotController::WantsRespawn() const -> bool
{
    const auto wait = std::chrono::seconds(settings::get<uint16>("map.PLAYER_BOTS_RAISE_WAIT"));
    return PBot->isDead() && m_deathTime != timer::time_point{} && timer::now() - m_deathTime >= wait;
}

auto CBotController::OnlineSince() const -> timer::time_point
{
    return m_onlineSince;
}

auto CBotController::WantsLogout() const -> bool
{
    return timer::now() >= m_logoutAt && PBot->isAlive() && PBot->PParty == nullptr && !PBot->PAI->IsEngaged() &&
           PBot->InvitePending.entity.UniqueNo == 0;
}

void CBotController::SetHome(const position_t& home)
{
    m_home = home;
}

void CBotController::ApplyTpMode()
{
    // In a party the bot closes skillchains real members open (it fires anyway at 3000 TP); solo it never waits.
    m_tpForParty          = PBot->PParty != nullptr;
    m_gambits->tp_trigger = m_tpForParty ? gambits::G_TP_TRIGGER::CLOSER : gambits::G_TP_TRIGGER::ASAP;
}

auto CBotController::FollowTarget() const -> uint32
{
    return m_followLeader;
}

void CBotController::JoinedByFollow(const uint32 leaderId)
{
    m_inParty       = true;
    m_partyLeaderId = leaderId;
    m_hunting       = false;
    m_prey          = {};
    ApplyTpMode();
}

void CBotController::QueueInvite()
{
    m_inviteAt = timer::now() + std::chrono::seconds(xirand::GetRandomNumber(2, 7));
}

void CBotController::QueueTellReply(const uint32 senderId)
{
    // One answer at a time: a burst of tells gets a single reply.
    if (m_tellFrom != 0)
    {
        return;
    }

    m_tellFrom = senderId;
    m_tellAt   = timer::now() + std::chrono::seconds(xirand::GetRandomNumber(2, 6));
}

auto CBotController::Tick(const timer::time_point tick) -> Task<void>
{
    if (PBot->InvitePending.entity.UniqueNo != 0 && tick >= m_inviteAt)
    {
        DecideInvite();
    }

    if (m_tellFrom != 0 && tick >= m_tellAt)
    {
        AnswerTell();
    }

    m_followLeader = 0;

    // Dismissed, or the party broke up: say goodbye to whoever led it and carry on alone.
    if (m_inParty && PBot->PParty == nullptr)
    {
        if (auto* PLeader = zoneutils::GetChar(m_partyLeaderId); PLeader && PLeader->loc.zone == PBot->loc.zone)
        {
            botutils::Chat(PBot, "dismissed", botutils::ChatChannel::Tell, PLeader);
        }

        BackToSolo();
    }

    if (PBot->PParty != nullptr)
    {
        if (const auto* PLeader = PartyLeaderHere())
        {
            m_partyLeaderId   = PLeader->id;
            m_leaderGoneSince = {};
        }
        else
        {
            LeaderGone(tick);
            if (m_followLeader != 0)
            {
                co_return;
            }
        }
    }

    // Level sync (or a level up) changes what the bot can cast and which weapon skills it has.
    if (PBot->GetMLevel() != m_lastLevel)
    {
        m_lastLevel = PBot->GetMLevel();
        RefreshSkills();
    }

    if (m_tpForParty != (PBot->PParty != nullptr))
    {
        ApplyTpMode();
    }

    if (PBot->isDead())
    {
        DeadTick(tick);
        co_return;
    }

    m_deathTime = {};

    // Attacked first (aggro, or a mob that hit it while it walked or rested): fight back.
    if (!PBot->PAI->IsEngaged() && !isBusy(PBot) && tick >= m_nextAttackerCheck)
    {
        m_nextAttackerCheck = tick + 1s;
        if (auto* PAttacker = Attacker())
        {
            if (IsResting())
            {
                StopResting();
            }

            PBot->PAI->PathFind->Clear();
            Engage(PAttacker->entityId());
        }
    }

    if (PBot->PAI->IsEngaged())
    {
        co_await CombatTick(tick);
    }
    else if (PBot->PParty != nullptr)
    {
        co_await PartyTick(tick);
    }
    else
    {
        co_await SoloTick(tick);
    }

    co_return;
}

void CBotController::DecideInvite()
{
    const uint32 inviterId = PBot->InvitePending.entity.UniqueNo;
    PBot->InvitePending.clean();

    auto* PInviter = zoneutils::GetChar(inviterId);
    if (PInviter == nullptr || PBot->isDead())
    {
        return;
    }

    // Invited from /sea by someone in another zone: a bot cannot cross the world, so it says where it is.
    if (PInviter->loc.zone != PBot->loc.zone)
    {
        PInviter->pushPacket<GP_SERV_COMMAND_MESSAGE>(PInviter, 0, 0, MsgStd::InvitationDeclined);
        botutils::Chat(PBot, "refuse_far", botutils::ChatChannel::Tell, PInviter);
        return;
    }

    auto* PParty = PInviter->PParty;

    // The level rule holds for the whole party: its highest real member, not only the inviter.
    uint8 highest = PInviter->GetMLevel();
    uint8 bots    = 0;
    if (PParty != nullptr)
    {
        for (auto* PMember : PParty->members)
        {
            if (botutils::IsBot(PMember))
            {
                ++bots;
            }
            else if (PMember->objtype == TYPE_PC)
            {
                highest = std::max(highest, PMember->GetMLevel());
            }
        }
    }

    const int16 gap = static_cast<int16>(highest) - static_cast<int16>(PBot->GetMLevel());

    std::string refusal;
    if (PBot->PParty != nullptr)
    {
        refusal = "refuse_busy";
    }
    else if (PParty && PParty->m_PAlliance != nullptr)
    {
        refusal = "refuse_alliance";
    }
    else if (PParty && (PParty->GetLeader() != PInviter || PParty->IsFull()))
    {
        refusal = "refuse_full";
    }
    else if (bots >= settings::get<uint8>("map.PLAYER_BOTS_MAX_PER_PARTY"))
    {
        refusal = "refuse_bots";
    }
    else if (!botutils::FitsPartyLevel(highest, PBot->GetMLevel()))
    {
        refusal = gap > 0 ? "refuse_high" : "refuse_low";
    }
    else if (botutils::FindSeeker(PBot->loc.zone, PInviter, highest) != nullptr)
    {
        refusal = "refuse_seeker";
    }

    if (!refusal.empty())
    {
        ShowDebugFmt("Player bots: {} turns down {}'s invite ({})", PBot->getName(), PInviter->getName(), refusal);
        PInviter->pushPacket<GP_SERV_COMMAND_MESSAGE>(PInviter, 0, 0, MsgStd::InvitationDeclined);
        botutils::Chat(PBot, refusal, botutils::ChatChannel::Tell, PInviter);
        return;
    }

    if (PParty == nullptr)
    {
        PInviter->PParty = new CParty(PInviter); // cpp.sh allow
    }

    PInviter->PParty->AddMember(PBot);
    if (PBot->PParty == nullptr)
    {
        ShowWarningFmt("Player bots: {} could not join {}'s party", PBot->getName(), PInviter->getName());
        return;
    }

    m_inParty       = true;
    m_partyLeaderId = PInviter->id;
    m_hunting       = false;
    m_prey          = {};

    if (IsResting())
    {
        StopResting();
    }

    PBot->PAI->PathFind->Clear();
    botutils::Chat(PBot, "join", botutils::ChatChannel::Party);
}

void CBotController::AnswerTell()
{
    const uint32 senderId = m_tellFrom;
    m_tellFrom            = 0;

    auto* PSender = zoneutils::GetChar(senderId);
    if (PSender == nullptr)
    {
        return;
    }

    // What the bot says depends on what it is doing; a bot seeking a party asks for an invite.
    std::string event = "tell";
    if (PBot->PParty != nullptr)
    {
        event = "tell_party";
    }
    else if (m_hunting || PBot->PAI->IsEngaged())
    {
        event = "tell_hunting";
    }
    else if (PBot->playerConfig.InviteFlg)
    {
        event = "tell_seeking";
    }
    else if (PBot->loc.zone != nullptr && (PBot->loc.zone->GetTypeMask() & xi::ZoneType::City) != xi::ZoneType::Unknown)
    {
        event = "tell_town";
    }

    botutils::Chat(PBot, event, botutils::ChatChannel::Tell, PSender);
}

auto CBotController::TakeInvite(const uint32 inviterId) -> bool
{
    // One invite a minute from each player; more get no answer at all, so nobody can spam a bot's tells.
    const auto now = timer::now();
    std::erase_if(m_lastInvite, [now](const auto& entry)
                  {
                      return now - entry.second >= InviteRepeatWait;
                  });

    if (m_lastInvite.contains(inviterId))
    {
        return false;
    }

    m_lastInvite[inviterId] = now;
    return true;
}

void CBotController::HelpStranger(const timer::time_point tick)
{
    if (tick < m_nextGoodDeed || PBot->loc.zone == nullptr)
    {
        return;
    }

    m_nextGoodDeed = tick + GoodDeedCheck;
    std::erase_if(m_helped, [tick](const auto& entry)
                  {
                      return tick - entry.second >= GoodDeedRepeatWait;
                  });

    // Now and then, not every time: a passer-by, not a bodyguard.
    if (xirand::GetRandomNumber(GoodDeedOdds) != 0)
    {
        return;
    }

    const auto raise = m_spells->GetBestAvailable(SPELLFAMILY_RAISE);
    const auto cure  = m_spells->GetBestAvailable(SPELLFAMILY_CURE);
    if (!raise && !cure)
    {
        return;
    }

    for (auto* PPlayer : botutils::RealPlayers(PBot->loc.zone))
    {
        if (m_helped.contains(PPlayer->id) || (PBot->PParty != nullptr && PPlayer->PParty == PBot->PParty) ||
            botutils::IgnoresBot(PPlayer, PBot) || ::distance(PBot->loc.p, PPlayer->loc.p) > GoodDeedRange)
        {
            continue;
        }

        Maybe<SpellID> spell;
        if (PPlayer->isDead() && raise && !PPlayer->m_hasRaise)
        {
            spell = raise;
        }
        else if (PPlayer->isAlive() && cure && PPlayer->GetHPP() < GoodDeedHPP)
        {
            spell = cure;
        }

        if (spell && Cast(PPlayer->entityId(), *spell))
        {
            ShowDebugFmt("Player bots: {} helps {} in passing", PBot->getName(), PPlayer->getName());
            m_helped[PPlayer->id] = tick;
            return;
        }
    }
}

auto CBotController::PartyLeaderHere() const -> CCharEntity*
{
    if (PBot->PParty == nullptr)
    {
        return nullptr;
    }

    auto* PLeader = dynamic_cast<CCharEntity*>(PBot->PParty->GetLeader());
    if (PLeader == nullptr || PLeader == PBot || PLeader->isBot() || PLeader->loc.zone != PBot->loc.zone ||
        PLeader->status == xi::Status::Disappear || !std::ranges::contains(PBot->PParty->members, PLeader))
    {
        return nullptr;
    }

    return PLeader;
}

void CBotController::LeaderGone(const timer::time_point tick)
{
    if (m_leaderGoneSince == timer::time_point{})
    {
        m_leaderGoneSince = tick;
    }

    // The leader has arrived in another zone, still in this party: go with it (custom).
    auto* PLeader = zoneutils::GetChar(m_partyLeaderId);
    if (PBot->isAlive() && PLeader != nullptr && PLeader->status != xi::Status::Disappear && PLeader->loc.zone != nullptr &&
        PLeader->loc.zone != PBot->loc.zone && PLeader->PParty == PBot->PParty && botutils::MayFollowInto(PLeader))
    {
        m_followLeader = PLeader->id;
        return;
    }

    // The leader is online but in another party, or in none for longer than a zone-in takes to reload it, or went
    // somewhere bots cannot follow and stayed: the party is over for the bot. A leader on the way (zoning), or in
    // a Mog House for a moment, gets some time.
    const bool online    = PLeader != nullptr && PLeader->status != xi::Status::Disappear;
    const bool leftParty = online && (PLeader->PParty != nullptr ? PLeader->PParty != PBot->PParty : tick - m_leaderGoneSince >= PartyReloadWait);
    if (leftParty || tick - m_leaderGoneSince >= LeaderWait)
    {
        LeaveParty();
    }
}

void CBotController::LeaveParty()
{
    ShowDebugFmt("Player bots: {} leaves its party (leader gone or out of reach)", PBot->getName());
    botutils::Chat(PBot, "leave", botutils::ChatChannel::Party);
    botutils::LeaveParty(PBot);
    BackToSolo();
}

void CBotController::BackToSolo()
{
    m_inParty         = false;
    m_partyLeaderId   = 0;
    m_leaderGoneSince = {};
    m_farSince        = {};
    m_home            = PBot->loc.p;
    m_nextHuntCheck   = timer::now() + 10s;
    m_stopUntil       = timer::now() + std::chrono::seconds(xirand::GetRandomNumber(10, 30));

    // Bots that were looking for a party when they arrived look again.
    if (const auto* PSpec = botutils::GetSpec(PBot->id); PSpec && PSpec->seeking && !PBot->playerConfig.InviteFlg)
    {
        PBot->playerConfig.InviteFlg = true;
        PBot->updatemask |= UPDATE_HP;
    }
}

auto CBotController::PartyTick(const timer::time_point tick) -> Task<void>
{
    auto* PLeader = PartyLeaderHere();
    if (PLeader == nullptr)
    {
        co_return;
    }

    // Assist: fight what the leader fights.
    if (PLeader->PAI->IsEngaged())
    {
        auto* PTarget = PLeader->GetBattleTarget();
        if (PTarget != nullptr && PTarget->isAlive() && ::distance(PBot->loc.p, PTarget->loc.p) <= AssistRange && !isBusy(PBot))
        {
            if (IsResting())
            {
                StopResting();
            }

            PBot->PAI->PathFind->Clear();
            Engage(PTarget->entityId());
            co_return;
        }
    }

    // Rest when the leader rests, and on its own between fights when it runs low on MP (a healer or caster that
    // never rested would soon have nothing left to cast).
    const bool leaderResting = PLeader->StatusEffectContainer->HasStatusEffect(xi::StatusEffect::Healing);
    const bool full          = PBot->GetHPP() >= 100 && (PBot->GetMaxMP() == 0 || PBot->GetMPP() >= 100);
    const bool lowMP         = PBot->GetMaxMP() > 0 && PBot->GetMPP() < PartyRestMPP;
    if (IsResting())
    {
        if (full || (!leaderResting && !m_restingForMP))
        {
            StopResting();
        }

        co_return;
    }

    if ((leaderResting || lowMP) && !full && !isBusy(PBot))
    {
        StartResting();
        m_restingForMP = !leaderResting;
        co_return;
    }

    // Follow the leader, keeping a little space: run there at any distance, as a player would.
    // A path around the leader stops a little short of FollowDistance, so only start one past FollowSlack, or the
    // bot would path again every tick and never stand still long enough to cure or buff.
    const float range = ::distance(PBot->loc.p, PLeader->loc.p);
    if (range > FollowDistance + FollowSlack && !isBusy(PBot))
    {
        if (!PBot->PAI->PathFind->IsFollowingPath() || ::distance(PBot->PAI->PathFind->GetDestination(), PLeader->loc.p) > FollowDistance + 2.0f)
        {
            // No path at all (the leader stands off the navmesh): warping is the only way there.
            if (!PBot->PAI->PathFind->PathAround(PLeader->loc.p, FollowDistance, PATHFLAG_RUN))
            {
                PBot->PAI->PathFind->WarpTo(PLeader->loc.p);
            }
        }
    }

    // Stuck out of reach for a minute: give up on the party rather than stand there.
    if (range <= AssistRange)
    {
        m_farSince = {};
    }
    else if (m_farSince == timer::time_point{})
    {
        m_farSince = tick;
    }
    else if (tick - m_farSince >= ReachTimeout)
    {
        LeaveParty();
        co_return;
    }

    if (PBot->PAI->PathFind->IsFollowingPath())
    {
        PBot->PAI->PathFind->FollowPath(tick);
    }
    else
    {
        // Cures and buffs between fights, as a trust does, only while standing still.
        co_await m_gambits->Tick(tick);
    }

    co_return;
}

void CBotController::DeadTick(const timer::time_point tick)
{
    m_hunting = false;
    m_prey    = {};

    if (m_deathTime == timer::time_point{})
    {
        m_deathTime = tick;

        // A dead bot holds nothing: the mob is free for anyone at once.
        botutils::DropClaims(PBot);

        if (PBot->PParty != nullptr)
        {
            botutils::Chat(PBot, "death", botutils::ChatChannel::Party);
        }
    }

    // Nobody answers the raise menu for a bot, so it takes every raise it is offered.
    if (PBot->m_hasRaise != 0)
    {
        PBot->Raise();
        m_deathTime = {};
    }
}

auto CBotController::CombatTick(const timer::time_point tick) -> Task<void>
{
    auto* PTarget = PBot->GetBattleTarget();
    if (PTarget == nullptr || PTarget->isDead())
    {
        // Hunt on from where the last fight ended, as players drift across a zone.
        m_home = PBot->loc.p;
        Disengage();
        m_hunting = false;
        m_prey    = {};
        co_return;
    }

    if (!isBusy(PBot))
    {
        const float range    = IsCaster() ? CasterRange : PBot->GetMeleeRange(PTarget) - 0.5f;
        const float distance = ::distance(PBot->loc.p, PTarget->loc.p);
        if (distance > range)
        {
            if (!PBot->PAI->PathFind->IsFollowingPath() || ::distance(PBot->PAI->PathFind->GetDestination(), PTarget->loc.p) > range + 2.0f)
            {
                PBot->PAI->PathFind->PathAround(PTarget->loc.p, std::max(range - 1.0f, 1.0f), PATHFLAG_RUN);
            }

            PBot->PAI->PathFind->FollowPath(tick);
        }
        else
        {
            PBot->PAI->PathFind->Clear();
            PBot->PAI->PathFind->LookAt(PTarget->loc.p);
        }
    }

    co_await m_gambits->Tick(tick);

    co_return;
}

auto CBotController::SoloTick(const timer::time_point tick) -> Task<void>
{
    if (IsResting())
    {
        if (PBot->GetHPP() >= 100 && (PBot->GetMaxMP() == 0 || PBot->GetMPP() >= 100))
        {
            StopResting();
        }

        co_return;
    }

    if (ShouldRest())
    {
        StartResting();
        co_return;
    }

    if (isBusy(PBot))
    {
        co_return;
    }

    if (!m_prey.isSet())
    {
        HelpStranger(tick);
        if (isBusy(PBot))
        {
            co_return;
        }
    }

    if (m_prey.isSet())
    {
        auto* PMob = m_prey.resolve<CMobEntity>();
        if (PMob == nullptr || PMob->isDead() || (PMob->m_OwnerID.isSet() && !(PMob->m_OwnerID == PBot)) ||
            ::distance(PBot->loc.p, PMob->loc.p) > PreySearchRange * 1.5f)
        {
            m_prey    = {};
            m_hunting = false;
        }
        else if (::distance(PBot->loc.p, PMob->loc.p) <= EngageRange)
        {
            PBot->PAI->PathFind->Clear();

            // Look again just before the pull: a player may have come for it on the way.
            if (!botutils::IsFairPrey(PBot, PMob))
            {
                m_prey    = {};
                m_hunting = false;
                co_return;
            }

            Engage(m_prey);
            co_return;
        }
        else
        {
            if (!PBot->PAI->PathFind->IsFollowingPath() || ::distance(PBot->PAI->PathFind->GetDestination(), PMob->loc.p) > EngageRange)
            {
                PBot->PAI->PathFind->PathInRange(PMob->loc.p, EngageRange - 2.0f, PATHFLAG_RUN);
            }

            PBot->PAI->PathFind->FollowPath(tick);
            co_return;
        }
    }

    if (tick >= m_nextHuntCheck)
    {
        m_nextHuntCheck       = tick + 5s;
        m_hunting             = false;
        const bool nearPlayer = PlayerWithin(NearPlayerRange);
        if (botutils::MayHunt(PBot) && (!nearPlayer || xirand::GetRandomNumber(NearPlayerHuntOdds) == 0))
        {
            if (auto* PMob = FindPrey())
            {
                m_prey    = PMob->entityId();
                m_hunting = true;
                co_return;
            }
        }
    }

    // On the way to the next stop.
    if (PBot->PAI->PathFind->IsFollowingPath())
    {
        PBot->PAI->PathFind->FollowPath(tick);
        co_return;
    }

    // At a stop: a real player lingering right next to it makes it move on early.
    if (PlayerWithin(CrowdRange))
    {
        if (m_crowdedSince == timer::time_point{})
        {
            m_crowdedSince = tick;
        }
        else if (tick - m_crowdedSince >= CrowdPatience)
        {
            m_stopUntil = tick;
        }
    }
    else
    {
        m_crowdedSince = {};
    }

    if (tick >= m_stopUntil)
    {
        m_crowdedSince = {};
        m_stopUntil    = tick + (StartJourney() ? StopTime() : 10s);
    }
    else if (tick >= m_nextRoam)
    {
        // Shuffle about a little while waiting, as people do.
        m_nextRoam = tick + std::chrono::seconds(xirand::GetRandomNumber(15, 40));
        PBot->PAI->PathFind->RoamAround(m_home, StopWanderRadius, 1, 2);
    }

    PBot->PAI->PathFind->FollowPath(tick);

    co_return;
}

auto CBotController::Attacker() const -> CBattleEntity*
{
    if (PBot->loc.zone == nullptr)
    {
        return nullptr;
    }

    // Mobs fighting the bot need not be in its notoriety list (that is filled for some kinds of hate only), so look
    // at who the mobs are fighting.
    CBattleEntity* PNearest = nullptr;
    float          nearest  = std::numeric_limits<float>::max();
    PBot->loc.zone->ForEachMob(
        [&](CMobEntity* PMob)
        {
            if (PMob->isDead() || PMob->GetBattleTarget() != PBot)
            {
                return;
            }

            if (const float range = ::distance(PMob->loc.p, PBot->loc.p); range < nearest)
            {
                PNearest = PMob;
                nearest  = range;
            }
        });

    return PNearest;
}

auto CBotController::PlayerWithin(const float range) const -> bool
{
    return std::ranges::any_of(botutils::RealPlayers(PBot->loc.zone), [&](const CCharEntity* PChar)
                               {
                                   return ::distance(PChar->loc.p, PBot->loc.p) < range;
                               });
}

auto CBotController::StopTime() const -> timer::duration
{
    // Cities are for standing about; out in the field bots keep moving.
    const bool city = PBot->loc.zone != nullptr && (PBot->loc.zone->GetTypeMask() & xi::ZoneType::City) != xi::ZoneType::Unknown;
    return city ? std::chrono::seconds(xirand::GetRandomNumber(60, 300)) : std::chrono::seconds(xirand::GetRandomNumber(20, 90));
}

auto CBotController::StartJourney() -> bool
{
    auto* PZone = PBot->loc.zone;
    if (PZone == nullptr || PZone->navMesh() == nullptr)
    {
        return false;
    }

    // Outdoors, half the time the next stop is a camp of mobs the bot's level (where a player would go to fight).
    std::vector<position_t> camps;
    if ((PZone->GetTypeMask() & xi::ZoneType::City) == xi::ZoneType::Unknown && xirand::GetRandomNumber(2) == 0)
    {
        PZone->ForEachMob(
            [&](CMobEntity* PMob)
            {
                const float range = ::distance(PBot->loc.p, PMob->m_SpawnPoint);
                const auto  check = charutils::CheckMob(PBot->GetMLevel(), PMob);
                if (range >= JourneyMin && range <= JourneyMax && check >= EMobDifficulty::EasyPrey && check <= EMobDifficulty::EvenMatch)
                {
                    camps.push_back(PMob->m_SpawnPoint);
                }
            });
    }

    for (int attempt = 0; attempt < 8; ++attempt)
    {
        Maybe<position_t> stop;
        if (!camps.empty())
        {
            stop = camps[xirand::GetRandomNumber(camps.size())];
        }
        else
        {
            stop = PZone->navMesh()->findRandomPosition(PBot->loc.p, JourneyMax);
        }

        if (!stop.has_value() || ::distance(PBot->loc.p, *stop) < JourneyMin * (camps.empty() ? 0.5f : 1.0f))
        {
            continue;
        }

        if (PBot->PAI->PathFind->PathTo(*stop, PATHFLAG_RUN))
        {
            m_home = *stop;
            return true;
        }
    }

    return false;
}

auto CBotController::FindPrey() -> CMobEntity*
{
    if (PBot->loc.zone == nullptr)
    {
        return nullptr;
    }

    CMobEntity* PBest     = nullptr;
    float       bestRange = PreySearchRange;

    PBot->loc.zone->ForEachMob(
        [&](CMobEntity* PMob)
        {
            const float range = ::distance(PBot->loc.p, PMob->loc.p);
            if (range >= bestRange)
            {
                return;
            }

            const auto check = charutils::CheckMob(PBot->GetMLevel(), PMob);
            if (check < EMobDifficulty::EasyPrey || check > EMobDifficulty::EvenMatch || !botutils::IsFairPrey(PBot, PMob))
            {
                return;
            }

            PBest     = PMob;
            bestRange = range;
        });

    return PBest;
}

auto CBotController::ShouldRest() const -> bool
{
    const bool lowMP = PBot->GetMaxMP() > 0 && PBot->GetMPP() < 40;
    return PBot->GetHPP() < 50 || lowMP;
}

auto CBotController::IsResting() const -> bool
{
    return PBot->StatusEffectContainer->HasStatusEffect(xi::StatusEffect::Healing);
}

void CBotController::StartResting()
{
    PBot->PAI->PathFind->Clear();
    const auto healingTickDelay = std::chrono::seconds(settings::get<uint8>("map.HEALING_TICK_DELAY"));
    PBot->StatusEffectContainer->AddStatusEffect(xi::StatusEffect::Healing, 0, 0, healingTickDelay, 0s);
}

void CBotController::StopResting()
{
    PBot->StatusEffectContainer->DelStatusEffectSilent(xi::StatusEffect::Healing);
}

auto CBotController::IsCaster() const -> bool
{
    switch (PBot->GetMJob())
    {
        case xi::Job::WHM:
        case xi::Job::BLM:
        case xi::Job::SMN:
        case xi::Job::SCH:
        case xi::Job::GEO:
            return true;
        default:
            return false;
    }
}
