/*
===========================================================================

  Copyright (c) 2010-2015 Darkstar Dev Teams

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTItem or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see http://www.gnu.org/licenses/

===========================================================================
*/

#include "campaign_system.h"
#include "packets/s2c/0x071_influence_campaign.h"
#include "utils/charutils.h"

namespace
{

CampaignState campaignState;

} // namespace

namespace campaign
{

// Each map process keeps its own copy, so the state is read back from the database (which every process writes
// through CCampaignHandler and the Set* functions below) rather than from the zones this process happens to run.
void LoadNations()
{
    std::vector<CampaignNation> nations;

    const auto rset = db::preparedStmt("SELECT id, reconnaissance, morale, prosperity FROM campaign_nation ORDER BY id ASC");
    if (rset && rset->rowsCount())
    {
        while (rset->next())
        {
            CampaignNation nation;
            nation.reconnaissance = rset->get<uint8>("reconnaissance");
            nation.morale         = rset->get<uint8>("morale");
            nation.prosperity     = rset->get<uint8>("prosperity");
            nations.emplace_back(nation);
        }
    }

    // Seven armies: the three nations and the four beastmen armies (CampaignArmy).
    nations.resize(7);
    campaignState.nations = std::move(nations);
}

void LoadState()
{
    CampaignState state;
    state.nations = std::move(campaignState.nations);

    const auto rset = db::preparedStmt("SELECT id, isbattle, nation, heroism, influence_sandoria, influence_bastok, influence_windurst, influence_beastman, "
                                       "current_fortifications, current_resources, max_fortifications, max_resources FROM campaign_map ORDER BY id ASC");
    if (rset && rset->rowsCount())
    {
        while (rset->next())
        {
            CampaignRegion region;
            region.campaignId = rset->get<uint8>("id");
            region.status     = rset->get<uint8>("isbattle");
            // `nation` holds the client's owner byte (CampaignControl: 2 San d'Oria, 4 Bastok, 6 Windurst,
            // 8 beastmen), i.e. the owner shifted past the battle bit; 0 is nobody.
            region.nationControl         = rset->get<uint8>("nation") >> 1;
            region.heroism               = rset->get<uint8>("heroism");
            region.influenceSandoria     = rset->get<uint8>("influence_sandoria");
            region.influenceBastok       = rset->get<uint8>("influence_bastok");
            region.influenceWindurst     = rset->get<uint8>("influence_windurst");
            region.influenceBeastman     = rset->get<uint8>("influence_beastman");
            region.currentFortifications = rset->get<uint16>("current_fortifications");
            region.currentResources      = rset->get<uint16>("current_resources");
            region.maxFortifications     = rset->get<uint16>("max_fortifications");
            region.maxResources          = rset->get<uint16>("max_resources");

            switch (region.nationControl)
            {
                case CampaignControl::SandoriaMask >> 1:
                    state.controlSandoria += 1;
                    break;
                case CampaignControl::BastokMask >> 1:
                    state.controlBastok += 1;
                    break;
                case CampaignControl::WindurstMask >> 1:
                    state.controlWindurst += 1;
                    break;
                case CampaignControl::BeastmanMask >> 1:
                    state.controlBeastman += 1;
                    break;
                default:
                    break;
            }

            state.regions.emplace_back(region);
        }
    }

    campaignState = std::move(state);
}

auto GetCampaignState() -> CampaignState
{
    LoadNations();
    LoadState();
    return campaignState;
}

auto GetReconnaissance(CampaignArmy army) -> uint8
{
    return campaignState.nations[army].reconnaissance;
}

auto GetMorale(CampaignArmy army) -> uint8
{
    return campaignState.nations[army].morale;
}

auto GetProsperity(CampaignArmy army) -> uint8
{
    return campaignState.nations[army].prosperity;
}

auto GetAlliedNotes(CCharEntity* chr) -> int32
{
    return charutils::GetPoints(chr, "allied_notes");
}

void SetReconnaissance(CampaignArmy army, int8 amount)
{
    const auto current = std::clamp((int32)amount, 0, 10);

    const auto rset = db::preparedStmt("UPDATE `campaign_nation` SET `reconnaissance` = ? WHERE `id` = ?", current, (int32)army);
    if (!rset)
    {
        ShowError("Unable to update nation reconnaissance.");
        return;
    }
    campaignState.nations[army].reconnaissance = current;
}

void SetMorale(CampaignArmy army, int8 amount)
{
    const auto current = std::clamp((int32)amount, 0, 100);

    const auto rset = db::preparedStmt("UPDATE `campaign_nation` SET `morale` = ? WHERE `id` = ?", current, (int32)army);
    if (!rset)
    {
        ShowError("Unable to update nation morale.");
        return;
    }
    campaignState.nations[army].morale = current;
}

void SetProsperity(CampaignArmy army, int8 amount)
{
    const auto current = std::clamp((int32)amount, 0, 100);

    const auto rset = db::preparedStmt("UPDATE `campaign_nation` SET `prosperity` = ? WHERE `id` = ?", current, (int32)army);
    if (!rset)
    {
        ShowError("Unable to update nation prosperity.");
        return;
    }
    campaignState.nations[army].prosperity = current;
}

void SetAlliedNotes(CCharEntity* chr, int32 amount)
{
    charutils::SetPoints(chr, "allied_notes", amount);
}

void SendUpdate(CCharEntity* PChar)
{
    GetCampaignState();
    PChar->pushPacket<GP_SERV_COMMAND_INFLUENCE::CAMPAIGN>(PChar, campaignState, 0);
    PChar->pushPacket<GP_SERV_COMMAND_INFLUENCE::CAMPAIGN>(PChar, campaignState, 1);
}

}; // namespace campaign
