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
#include "packets/s2c/0x08f_trust_ascension_equip.h"
#include "utils/charutils.h"

#include <cstring>
#include <unordered_map>

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
    PChar->m_alterEgo  = {};
    PChar->m_celestite = {};

    const auto rset = db::preparedStmt("SELECT alter_ego, celestite FROM char_trust_ascension WHERE charid = ? LIMIT 1", PChar->id);
    if (rset && rset->rowsCount() && rset->next())
    {
        db::extractFromBlob(rset, "alter_ego", PChar->m_alterEgo);
        db::extractFromBlob(rset, "celestite", PChar->m_celestite);
    }
}

void SaveCelestite(const CCharEntity* PChar)
{
    db::preparedStmt("INSERT INTO char_trust_ascension (charid, celestite) VALUES (?, ?) "
                     "ON DUPLICATE KEY UPDATE celestite = VALUES(celestite)",
                     PChar->id,
                     PChar->m_celestite);
}

auto HasCelestite(const CCharEntity* PChar, const uint16 itemId) -> bool
{
    if (itemId <= CelestiteItemBase || itemId > CelestiteItemBase + 0xFF)
    {
        return false;
    }

    const uint8 n = static_cast<uint8>(itemId - CelestiteItemBase);
    return (PChar->m_celestite.owned[n / 32] >> (n % 32)) & 1;
}

void AddCelestite(CCharEntity* PChar, const uint16 itemId)
{
    if (itemId <= CelestiteItemBase || itemId > CelestiteItemBase + 0xFF)
    {
        return;
    }

    const uint8 n = static_cast<uint8>(itemId - CelestiteItemBase);
    PChar->m_celestite.owned[n / 32] |= 1u << (n % 32);
    SaveCelestite(PChar);
}

void SetCelestiteSlotLevel(CCharEntity* PChar, const uint8 slot, const uint8 level)
{
    if (slot >= CelestiteSlots)
    {
        return;
    }

    for (uint8 set = 1; set <= CelestiteSets; set++)
    {
        PChar->m_celestite.sets[set][slot] = level;

        // UNVERIFIED: what retail does with a celestite in a slot that closes was not seen.
        if (level == 0)
        {
            PChar->m_celestite.sets[set][CelestiteSlots + slot] = 0;
        }
    }

    SaveCelestite(PChar);
}

void FlushPendingCelestite(CCharEntity* PChar)
{
    if (PChar->m_celestitePending)
    {
        PChar->m_celestitePending = false;
        PChar->pushPacket<GP_SERV_COMMAND_TRUST_ASCENSION_EQUIP>(PChar);
    }
}

auto SetCelestiteSet(CCharEntity* PChar, const uint8 set, const uint8 (&slots)[CelestiteSlots]) -> bool
{
    if (set < 1 || set > CelestiteSets)
    {
        return false;
    }

    for (uint8 i = 0; i < CelestiteSlots; i++)
    {
        const uint8 n = slots[i];
        if (n == 0)
        {
            continue;
        }

        if (PChar->m_celestite.sets[set][i] == 0 || !HasCelestite(PChar, CelestiteItemBase + n))
        {
            return false;
        }

        // A celestite may sit in every set, but only once within one set (client FUN_10117b70).
        for (uint8 j = 0; j < i; j++)
        {
            if (slots[j] == n)
            {
                return false;
            }
        }
    }

    std::memcpy(&PChar->m_celestite.sets[set][CelestiteSlots], slots, CelestiteSlots);
    SaveCelestite(PChar);

    return true;
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

namespace
{

// Trust spell id -> celestite set. Generated from the client's spell list (HD icon id / 9 = role).
const std::unordered_map<uint32, uint8> trustCelestiteSets = {
    { 896, 4 },  // Shantotto: Offensive Caster
    { 897, 2 },  // Naji: Melee Fighter
    { 898, 5 },  // Kupipi: Healer
    { 899, 2 },  // Excenmille: Melee Fighter
    { 900, 2 },  // Ayame: Melee Fighter
    { 901, 2 },  // Nanaa Mihgo: Melee Fighter
    { 902, 1 },  // Curilla: Tank
    { 903, 2 },  // Volker: Melee Fighter
    { 904, 4 },  // Ajido-Marujido: Offensive Caster
    { 905, 1 },  // Trion: Tank
    { 906, 2 },  // Zeid: Melee Fighter
    { 907, 2 },  // Lion: Melee Fighter
    { 908, 2 },  // Tenzen: Melee Fighter
    { 909, 5 },  // Mihli Aliapoh: Healer
    { 910, 1 },  // Valaineral: Tank
    { 911, 6 },  // Joachim: Support
    { 912, 2 },  // Naja Salaheem: Melee Fighter
    { 913, 2 },  // Prishe: Melee Fighter
    { 914, 6 },  // Ulmia: Support
    { 915, 2 },  // Shikaree Z: Melee Fighter
    { 916, 5 },  // Cherukiki: Healer
    { 917, 2 },  // Iron Eater: Melee Fighter
    { 918, 1 },  // Gessho: Tank
    { 919, 4 },  // Gadalar: Offensive Caster
    { 920, 2 },  // Rainemard: Melee Fighter
    { 921, 4 },  // Ingrid: Offensive Caster
    { 922, 2 },  // Lehko Habhoka: Melee Fighter
    { 923, 2 },  // Nashmeira: Melee Fighter
    { 924, 2 },  // Zazarg: Melee Fighter
    { 925, 4 },  // Ovjang: Offensive Caster
    { 926, 1 },  // Mnejing: Tank
    { 927, 0 },  // Sakura: Special
    { 928, 2 },  // Luzaf: Melee Fighter
    { 929, 3 },  // Najelith: Ranged Fighter
    { 930, 2 },  // Aldo: Melee Fighter
    { 931, 0 },  // Moogle: Special
    { 932, 2 },  // Fablinix: Melee Fighter
    { 933, 2 },  // Maat: Melee Fighter
    { 934, 4 },  // D. Shantotto: Offensive Caster
    { 935, 0 },  // Star Sibyl: Special
    { 936, 5 },  // Karaha-Baruha: Healer
    { 937, 2 },  // Cid: Melee Fighter
    { 938, 2 },  // Gilgamesh: Melee Fighter
    { 939, 2 },  // Areuhat: Melee Fighter
    { 940, 3 },  // Semih Lafihna: Ranged Fighter
    { 941, 3 },  // Elivira: Ranged Fighter
    { 942, 2 },  // Noillurie: Melee Fighter
    { 943, 2 },  // Lhu Mhakaracca: Melee Fighter
    { 944, 5 },  // Ferreous Coffin: Healer
    { 945, 2 },  // Lilisette: Melee Fighter
    { 946, 2 },  // Mumor: Melee Fighter
    { 947, 2 },  // Uka Totlihn: Melee Fighter
    { 948, 2 },  // Klara: Melee Fighter
    { 949, 2 },  // Romaa Mihgo: Melee Fighter
    { 950, 0 },  // Kuyin Hathdenna: Special
    { 951, 1 },  // Rahal: Tank
    { 952, 6 },  // Koru-Moru: Support
    { 953, 5 },  // Pieuje (UC): Healer
    { 954, 2 },  // I. Shield (UC): Melee Fighter
    { 955, 5 },  // Apururu (UC): Healer
    { 956, 2 },  // Jakoh (UC): Melee Fighter
    { 957, 2 },  // Flaviria (UC): Melee Fighter
    { 958, 2 },  // Babban: Melee Fighter
    { 959, 2 },  // Abenzio: Melee Fighter
    { 960, 1 },  // Rughadjeen: Tank
    { 961, 4 },  // Kukki-Chebukki: Offensive Caster
    { 962, 3 },  // Margret: Ranged Fighter
    { 963, 2 },  // Chacharoon: Melee Fighter
    { 964, 2 },  // Lhe Lhangavo: Melee Fighter
    { 965, 6 },  // Arciela: Support
    { 966, 2 },  // Mayakov: Melee Fighter
    { 967, 6 },  // Qultada: Support
    { 968, 4 },  // Adelheid: Offensive Caster
    { 969, 1 },  // Amchuchu: Tank
    { 970, 0 },  // Brygid: Special
    { 971, 2 },  // Mildaurion: Melee Fighter
    { 972, 2 },  // Halver: Melee Fighter
    { 973, 2 },  // Rongelouts: Melee Fighter
    { 974, 4 },  // Leonoyne: Offensive Caster
    { 975, 2 },  // Maximilian: Melee Fighter
    { 976, 4 },  // Kayeel-Payeel: Offensive Caster
    { 977, 4 },  // Robel-Akbel: Offensive Caster
    { 978, 0 },  // Kupofried: Special
    { 979, 2 },  // Selh'teus: Melee Fighter
    { 980, 5 },  // Yoran-Oran (UC): Healer
    { 981, 6 },  // Sylvie (UC): Support
    { 982, 2 },  // Abquhbah: Melee Fighter
    { 983, 2 },  // Balamor: Melee Fighter
    { 984, 1 },  // August: Tank
    { 985, 4 },  // Rosulatia: Offensive Caster
    { 986, 2 },  // Teodor: Melee Fighter
    { 987, 4 },  // Ullegore: Offensive Caster
    { 988, 3 },  // Makki-Chebukki: Ranged Fighter
    { 989, 6 },  // King of Hearts: Support
    { 990, 2 },  // Morimar: Melee Fighter
    { 991, 2 },  // Darrcuiln: Melee Fighter
    { 992, 1 },  // AAHM: Tank
    { 993, 1 },  // AAEV: Tank
    { 994, 2 },  // AAMR: Melee Fighter
    { 995, 4 },  // AATT: Offensive Caster
    { 996, 2 },  // AAGK: Melee Fighter
    { 997, 2 },  // Iroha: Melee Fighter
    { 998, 5 },  // Ygnas: Healer
    { 999, 5 },  // Monberaux: Healer
    { 1002, 0 }, // Cornelia: Special
    { 1003, 2 }, // Matsui-P: Melee Fighter
    { 1004, 2 }, // Excenmille [S]: Melee Fighter
    { 1005, 2 }, // Ayame (UC): Melee Fighter
    { 1006, 2 }, // Maat (UC): Melee Fighter
    { 1007, 2 }, // Aldo (UC): Melee Fighter
    { 1008, 2 }, // Naja (UC): Melee Fighter
    { 1009, 2 }, // Lion II: Melee Fighter
    { 1010, 2 }, // Zeid II: Melee Fighter
    { 1011, 2 }, // Prishe II: Melee Fighter
    { 1012, 2 }, // Nashmeira II: Melee Fighter
    { 1013, 2 }, // Lilisette II: Melee Fighter
    { 1014, 3 }, // Tenzen II: Ranged Fighter
    { 1015, 4 }, // Mumor II: Offensive Caster
    { 1016, 2 }, // Ingrid II: Melee Fighter
    { 1017, 6 }, // Arciela II: Support
    { 1018, 2 }, // Iroha II: Melee Fighter
    { 1019, 4 }, // Shantotto II: Offensive Caster
};

} // namespace

auto CelestiteSetForTrust(const uint32 trustSpellId) -> uint8
{
    const auto it = trustCelestiteSets.find(trustSpellId);
    return it != trustCelestiteSets.end() ? it->second : 0;
}

void ApplyCelestite(CTrustEntity* PTrust, const CCharEntity* PMaster)
{
    // Celestite are level 99 items (client item records) and work through the Celestite Case.
    if (!PMaster || PTrust->GetMLevel() < 99 || !charutils::hasKeyItem(PMaster, xi::KeyItem::CelestiteCase))
    {
        return;
    }

    const uint8 set = CelestiteSetForTrust(PTrust->trustID());
    if (set == 0)
    {
        return;
    }

    for (uint8 slot = 0; slot < CelestiteSlots; slot++)
    {
        const uint8 level     = PMaster->m_celestite.sets[set][slot];
        const uint8 celestite = PMaster->m_celestite.sets[set][CelestiteSlots + slot];
        if (level == 0 || level > 3 || celestite == 0)
        {
            continue; // UNVERIFIED: no values are known past slot level 3
        }

        for (const auto& effect : UnverifiedCelestiteEffects)
        {
            if (effect.celestite == celestite)
            {
                PTrust->addModifier(effect.mod, effect.bySlotLevel[level - 1]);
            }
        }
    }
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
