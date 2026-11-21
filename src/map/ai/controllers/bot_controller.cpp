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
#include "mob_spell_container.h"
#include "spell.h"
#include "status_effect_container.h"
#include "utils/battleutils.h"
#include "utils/botutils.h"
#include "utils/charutils.h"
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
{
    RefreshSkills();
}

CBotController::~CBotController() = default;

auto CBotController::GambitLeader() const -> CCharEntity*
{
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
    return 0;
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

auto CBotController::Tick(const timer::time_point tick) -> Task<void>
{
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
    else
    {
        co_await SoloTick(tick);
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
        Disengage();
        m_hunting = false;
        m_prey    = {};
        co_return;
    }

    if (ShouldFlee(PTarget))
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

    if (!PBot->PAI->PathFind->IsFollowingPath() && tick >= m_nextRoam)
    {
        m_nextRoam = tick + std::chrono::seconds(xirand::GetRandomNumber(10, 30));
        PBot->PAI->PathFind->RoamAround(m_home, RoamRadius, 1, 3);
    }

    PBot->PAI->PathFind->FollowPath(tick);

    co_return;
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
