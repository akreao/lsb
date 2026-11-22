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

namespace
{

// How far a solo bot looks for a mob, and how close a real player may be before it leaves that mob alone.
constexpr float PreySearchRange  = 40.0f;
constexpr float PlayerClearRange = 15.0f;
constexpr float EngageRange      = 20.0f;
constexpr float CasterRange      = 10.0f;
constexpr float RoamRadius       = 20.0f;

// A bot no real player can see (the client draws other characters within 45 yalms) heads toward one now and then,
// ending up this far from them: close enough to be seen, not on top of them.
constexpr float SeenRange    = 45.0f;
constexpr float RehomeMinGap = 15.0f;
constexpr float RehomeMaxGap = 35.0f;

// In a party: how close the bot keeps to its leader, how far it runs to join a fight, and when it warps.
constexpr float FollowDistance = 3.0f;
constexpr float AssistRange    = 30.0f;
constexpr float WarpDistance   = 50.0f;

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
, m_nextRehome(timer::now() + std::chrono::seconds(xirand::GetRandomNumber(2, 8)))
{
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
    m_gambits->tp_trigger = gambits::G_TP_TRIGGER::ASAP;
    m_gambits->tp_select  = gambits::G_SELECT::RANDOM;

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

void CBotController::SetHome(const position_t& home)
{
    m_home = home;
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

    // Dismissed, or the party broke up: say goodbye to whoever led it and carry on alone.
    if (m_inParty && PBot->PParty == nullptr)
    {
        if (auto* PLeader = zoneutils::GetChar(m_partyLeaderId); PLeader && PLeader->loc.zone == PBot->loc.zone)
        {
            botutils::Chat(PBot, "dismissed", botutils::ChatChannel::Tell, PLeader);
        }

        BackToSolo();
    }

    // The leader zoned out or logged out: the bot stays in its zone and leaves the party.
    if (PBot->PParty != nullptr && PartyLeaderHere() == nullptr)
    {
        LeaveParty();
    }

    if (PBot->isDead())
    {
        DeadTick(tick);
        co_return;
    }

    m_deathTime = {};

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
    if (PInviter == nullptr || PInviter->loc.zone != PBot->loc.zone || PBot->isDead())
    {
        return;
    }

    const int16 above = settings::get<uint8>("map.PLAYER_BOTS_PARTY_LEVEL_ABOVE");
    const int16 below = settings::get<uint8>("map.PLAYER_BOTS_PARTY_LEVEL_BELOW");
    const int16 gap   = static_cast<int16>(PInviter->GetMLevel()) - static_cast<int16>(PBot->GetMLevel());

    auto* PParty = PInviter->PParty;

    std::string refusal;
    if (PBot->PParty != nullptr)
    {
        refusal = "refuse_busy";
    }
    else if (gap > above)
    {
        refusal = "refuse_high";
    }
    else if (-gap > below)
    {
        refusal = "refuse_low";
    }
    else if (!PInviter->PTrusts.empty() || (PParty && PParty->HasTrusts()))
    {
        refusal = "refuse_trusts";
    }
    else if (PParty && PParty->m_PAlliance != nullptr)
    {
        refusal = "refuse_alliance";
    }
    else if (PParty && (PParty->GetLeader() != PInviter || PParty->IsFull()))
    {
        refusal = "refuse_full";
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
    m_fleeing       = false;
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

    if (auto* PSender = zoneutils::GetChar(senderId))
    {
        botutils::Chat(PBot, "tell", botutils::ChatChannel::Tell, PSender);
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

void CBotController::LeaveParty()
{
    ShowDebugFmt("Player bots: {} leaves its party (no leader in its zone)", PBot->getName());
    botutils::Chat(PBot, "leave", botutils::ChatChannel::Party);
    botutils::LeaveParty(PBot);
    BackToSolo();
}

void CBotController::BackToSolo()
{
    m_inParty       = false;
    m_partyLeaderId = 0;
    m_home          = PBot->loc.p;
    m_nextHuntCheck = timer::now() + 10s;

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

    // Rest when the leader rests.
    const bool leaderResting = PLeader->StatusEffectContainer->HasStatusEffect(xi::StatusEffect::Healing);
    const bool full          = PBot->GetHPP() >= 100 && (PBot->GetMaxMP() == 0 || PBot->GetMPP() >= 100);
    if (IsResting())
    {
        if (full || !leaderResting)
        {
            StopResting();
        }

        co_return;
    }

    if (leaderResting && !full && !isBusy(PBot))
    {
        StartResting();
        co_return;
    }

    // Follow the leader, keeping a little space.
    const float range = ::distance(PBot->loc.p, PLeader->loc.p);
    if (range > WarpDistance)
    {
        PBot->PAI->PathFind->WarpTo(PLeader->loc.p);
    }
    else if (range > FollowDistance && !isBusy(PBot))
    {
        if (!PBot->PAI->PathFind->IsFollowingPath() || ::distance(PBot->PAI->PathFind->GetDestination(), PLeader->loc.p) > 2.0f)
        {
            PBot->PAI->PathFind->PathAround(PLeader->loc.p, FollowDistance, PATHFLAG_RUN);
        }
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
    m_fleeing = false;
    m_prey    = {};

    if (m_deathTime == timer::time_point{})
    {
        m_deathTime = tick;

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

    // A bot in a party stands with it, as a player would.
    if (PBot->PParty == nullptr && ShouldFlee(PTarget))
    {
        Disengage();
        m_hunting = false;
        m_prey    = {};
        m_fleeing = true;
        PBot->PAI->PathFind->PathTo(m_home, PATHFLAG_RUN);
        co_return;
    }

    if (!isBusy(PBot))
    {
        const float range    = IsCaster() ? CasterRange : PBot->GetMeleeRange(PTarget) - 0.5f;
        const float distance = ::distance(PBot->loc.p, PTarget->loc.p);
        if (distance > range)
        {
            if (!PBot->PAI->PathFind->IsFollowingPath() || ::distance(PBot->PAI->PathFind->GetDestination(), PTarget->loc.p) > 2.0f)
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
    if (m_fleeing)
    {
        if (PBot->PAI->PathFind->IsFollowingPath())
        {
            PBot->PAI->PathFind->FollowPath(tick);
            co_return;
        }

        m_fleeing = false;
    }

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
            Engage(m_prey);
            co_return;
        }
        else
        {
            if (!PBot->PAI->PathFind->IsFollowingPath() || ::distance(PBot->PAI->PathFind->GetDestination(), PMob->loc.p) > 3.0f)
            {
                PBot->PAI->PathFind->PathInRange(PMob->loc.p, EngageRange - 2.0f, PATHFLAG_RUN);
            }

            PBot->PAI->PathFind->FollowPath(tick);
            co_return;
        }
    }

    if (tick >= m_nextHuntCheck)
    {
        m_nextHuntCheck = tick + 5s;
        m_hunting       = false;
        if (botutils::MayHunt(PBot))
        {
            if (auto* PMob = FindPrey())
            {
                m_prey    = PMob->entityId();
                m_hunting = true;
                co_return;
            }
        }
    }

    if (tick >= m_nextRehome)
    {
        Rehome(tick);
    }

    if (!PBot->PAI->PathFind->IsFollowingPath() && tick >= m_nextRoam)
    {
        m_nextRoam = tick + std::chrono::seconds(xirand::GetRandomNumber(10, 30));
        PBot->PAI->PathFind->RoamAround(m_home, RoamRadius, 1, 3);
    }

    PBot->PAI->PathFind->FollowPath(tick);

    co_return;
}

void CBotController::Rehome(const timer::time_point tick)
{
    m_nextRehome = tick + std::chrono::seconds(xirand::GetRandomNumber(30, 90));

    if (PBot->loc.zone == nullptr)
    {
        return;
    }

    std::vector<CCharEntity*> players;
    bool                      seen = false;
    PBot->loc.zone->ForEachChar(
        [&](CCharEntity* PChar)
        {
            if (PChar->status == xi::Status::Disappear || PChar->m_isGMHidden)
            {
                return;
            }

            players.push_back(PChar);
            seen = seen || ::distance(PChar->loc.p, PBot->loc.p) < SeenRange;
        });

    if (players.empty())
    {
        return;
    }

    // Already where people can see it: stroll somewhere else nearby, so the area looks busy and it never stands still long.
    if (seen)
    {
        if (PBot->PAI->PathFind->RoamAround(PBot->loc.p, RoamRadius * 4, 1, 3))
        {
            m_home = PBot->PAI->PathFind->GetDestination();
        }

        return;
    }

    const auto* PPlayer = players[xirand::GetRandomNumber(players.size())];
    const float gap     = xirand::GetRandomNumber(RehomeMinGap, RehomeMaxGap);
    if (PBot->PAI->PathFind->PathAround(PPlayer->loc.p, gap))
    {
        m_home     = PPlayer->loc.p;
        m_nextRoam = tick + std::chrono::seconds(xirand::GetRandomNumber(20, 40));
    }
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
            if (range >= bestRange || !PMob->isAlive() || !PMob->PAI->IsSpawned() || PMob->PAI->IsEngaged() ||
                PMob->m_OwnerID.isSet() || PMob->PMaster != nullptr || PMob->allegiance != xi::Allegiance::Mob ||
                (PMob->m_Type & xi::MobType::Notorious) != xi::MobType::Normal)
            {
                return;
            }

            const auto check = charutils::CheckMob(PBot->GetMLevel(), PMob);
            if (check < EMobDifficulty::EasyPrey || check > EMobDifficulty::EvenMatch)
            {
                return;
            }

            // Leave mobs near real players alone: they may want them.
            bool playerNear = false;
            PBot->loc.zone->ForEachChar(
                [&](CCharEntity* PChar)
                {
                    playerNear = playerNear || ::distance(PChar->loc.p, PMob->loc.p) < PlayerClearRange;
                });

            if (playerNear)
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

auto CBotController::ShouldFlee(const CBattleEntity* PTarget) const -> bool
{
    if (PBot->GetHPP() >= 25)
    {
        return false;
    }

    // Low HP against something its own level or above: run, as a player would.
    return charutils::CheckMob(PBot->GetMLevel(), const_cast<CBattleEntity*>(PTarget)) >= EMobDifficulty::EvenMatch;
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
