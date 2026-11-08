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

#include "0x071_influence_campaign.h"

#include "0x071_influence.h"
#include "campaign_system.h"

#include <algorithm>
#include <iterator>

GP_SERV_COMMAND_INFLUENCE::CAMPAIGN::CAMPAIGN(CCharEntity* PChar, const CampaignState& state, const uint8 number)
{
    auto& packet = this->data();

    packet.Mode        = GP_SERV_COMMAND_INFLUENCE_MODE::Campaign;
    packet.Length      = 0xC4; // This is the size of the rest of the packet after Length but without the padding.
    packet.AlliedNotes = campaign::GetAlliedNotes(PChar);

    packet.ControlledAreas.Sandoria = state.controlSandoria;
    packet.ControlledAreas.Bastok   = state.controlBastok;
    packet.ControlledAreas.Windurst = state.controlWindurst;
    packet.ControlledAreas.Beastman = state.controlBeastman;

    campaignnation_t* armies[] = {
        &packet.Nations.Sandoria,
        &packet.Nations.Bastok,
        &packet.Nations.Windurst,
        &packet.Nations.BeastmanOrc,
        &packet.Nations.BeastmanQuadav,
        &packet.Nations.BeastmanYagudo,
        &packet.Nations.BeastmanDarkKindred,
    };

    for (std::size_t i = 0; i < std::size(armies) && i < state.nations.size(); i++)
    {
        armies[i]->Reconnaissance = state.nations[i].reconnaissance;
        armies[i]->Morale         = state.nations[i].morale;
        armies[i]->Prosperity     = state.nations[i].prosperity;
    }

    // The client fills its 26 areas (campaign_map ids, in the client's own order) from two packets of 13.
    const std::size_t start = number == 0 ? 0 : 13;
    packet.ZoneOffset       = static_cast<uint8_t>(start);

    for (std::size_t i = start; i < start + 13 && i < state.regions.size(); i++)
    {
        const CampaignRegion& region = state.regions[i];
        auto&                 zone   = packet.Zones[i - start];

        zone.Battle                = region.status != 0 ? 1 : 0;
        zone.Owner                 = std::min<uint8>(region.nationControl, 4);
        zone.CurrentFortifications = std::min<uint16>(region.currentFortifications, 0x3FF);
        zone.CurrentResources      = std::min<uint16>(region.currentResources, 0x3FF);
        zone.Heroism               = region.heroism;
        zone.InfluenceSandoria     = region.influenceSandoria;
        zone.InfluenceBastok       = region.influenceBastok;
        zone.InfluenceWindurst     = region.influenceWindurst;
        zone.InfluenceBeastman     = region.influenceBeastman;
        zone.MaxFortifications     = std::min<uint16>(region.maxFortifications, 0x3FF);
        zone.MaxResources          = std::min<uint16>(region.maxResources, 0x3FF);
    }
}
