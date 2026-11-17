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

#include "trust_ascension.h"

#include "common/database.h"
#include "data/enums/key_item.h"
#include "entities/char_entity.h"
#include "entities/trust_entity.h"
#include "enums/alter_ego_points.h"
#include "enums/msg_basic.h"
#include "packets/s2c/0x029_battle_message.h"
#include "packets/s2c/0x08e_trust_ascension_points.h"
#include "utils/charutils.h"

namespace trustascension
{

auto AlterEgoCost(const uint8 level) -> uint16
{
    if (level >= MaxAlterEgoLevel)
    {
        return 0;
    }

    return static_cast<uint16>(level / 10 + 1);
}

void Load(CCharEntity* PChar)
{
    PChar->m_alterEgo = {};

    const auto rset = db::preparedStmt("SELECT alter_ego FROM char_trust_ascension WHERE charid = ? LIMIT 1", PChar->id);
    if (rset && rset->rowsCount() && rset->next())
    {
        db::extractFromBlob(rset, "alter_ego", PChar->m_alterEgo);
    }
}

void SaveAlterEgo(const CCharEntity* PChar)
{
    db::preparedStmt("INSERT INTO char_trust_ascension (charid, alter_ego) VALUES (?, ?) "
                     "ON DUPLICATE KEY UPDATE alter_ego = VALUES(alter_ego)",
                     PChar->id,
                     PChar->m_alterEgo);
}

auto UpgradeAlterEgo(CCharEntity* PChar, const uint8 category) -> bool
{
    if (category >= PChar->m_alterEgo.size())
    {
        return false;
    }

    const uint8  level  = PChar->m_alterEgo[category];
    const uint16 cost   = AlterEgoCost(level);
    const int32  points = charutils::GetPoints(PChar, "alter_ego_points");

    if (cost == 0 || points < cost)
    {
        return false;
    }

    PChar->m_alterEgo[category] = level + 1;
    SaveAlterEgo(PChar);
    charutils::SetPoints(PChar, "alter_ego_points", points - cost);

    // Retail answers each upgrade with message 828 (Data = attribute, Data2 = new level), then a fresh 0x08E.
    PChar->pushPacket<GP_SERV_COMMAND_BATTLE_MESSAGE>(PChar, PChar, category, level + 1, MsgBasic::AlterEgoUpgrade);
    PChar->pushPacket<GP_SERV_COMMAND_TRUST_ASCENSION_POINTS>(PChar);

    return true;
}

void ApplyAlterEgo(CTrustEntity* PTrust, const CCharEntity* PMaster)
{
    // Game text: the upgrades are "Valid from Lv. 99 or higher" and work through the Cipher Bracelet.
    if (!PMaster || PTrust->GetMLevel() < 99 || !charutils::hasKeyItem(PMaster, xi::KeyItem::CipherBracelet))
    {
        return;
    }

    const auto level = [&](AlterEgoCategory category) -> uint16
    {
        return PMaster->m_alterEgo[static_cast<uint8>(category)];
    };

    // Per level (game text): Max HP +5, Max MP +2, each attribute +1, each combat or magic skill +1.
    // UNVERIFIED placement: these are added after LSB's own trust calculation (and after the ALTER_EGO_*_MULTIPLIER
    // settings, which do not scale them). How retail combines them with a trust's own scaling is not known.
    PTrust->health.maxhp += 5 * level(AlterEgoCategory::HP);

    // UNVERIFIED: a trust without MP gets no MP from upgrades.
    if (PTrust->health.maxmp > 0)
    {
        PTrust->health.maxmp += 2 * level(AlterEgoCategory::MP);
    }

    PTrust->stats.STR += level(AlterEgoCategory::STR);
    PTrust->stats.DEX += level(AlterEgoCategory::DEX);
    PTrust->stats.VIT += level(AlterEgoCategory::VIT);
    PTrust->stats.AGI += level(AlterEgoCategory::AGI);
    PTrust->stats.INT += level(AlterEgoCategory::INT);
    PTrust->stats.MND += level(AlterEgoCategory::MND);
    PTrust->stats.CHR += level(AlterEgoCategory::CHR);

    // UNVERIFIED: only skills the trust already has are raised (the same ranges LSB gives trusts).
    const auto raiseSkills = [&](xi::SkillType first, xi::SkillType last, uint16 amount)
    {
        for (int i = static_cast<int>(first); i <= static_cast<int>(last); i++)
        {
            if (PTrust->WorkingSkills.skill[i] != 0)
            {
                PTrust->WorkingSkills.skill[i] += amount;
            }
        }
    };

    raiseSkills(xi::SkillType::HandToHand, xi::SkillType::Staff, level(AlterEgoCategory::CombatSkills));
    raiseSkills(xi::SkillType::DivineMagic, xi::SkillType::BlueMagic, level(AlterEgoCategory::MagicSkills));

    PTrust->UpdateHealth();
    PTrust->health.hp = PTrust->GetMaxHP();
    PTrust->health.mp = PTrust->GetMaxMP();
}

} // namespace trustascension
