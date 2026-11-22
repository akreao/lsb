/*
===========================================================================

  Copyright (c) 2010-2015 Darkstar Dev Teams

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

#include "lua_zone.h"

#include "common/logging.h"
#include "common/timer.h"

#include "entities/mob_entity.h"
#include "entities/npc_entity.h"
#include "lua/lua_base_entity.h"
#include "lua_base_entity.h"
#include "luautils.h"
#include "trigger_area.h"
#include "utils/botutils.h"
#include "ximesh/ximesh.h"
#include "zone.h"

CLuaZone::CLuaZone(CZone* PZone)
: m_pLuaZone(PZone)
{
    if (PZone == nullptr)
    {
        ShowError("CLuaZone created with nullptr instead of valid CZone*!");
    }
}

/************************************************************************
 *  Function: getLocalVar()
 *  Purpose : Returns a variable assigned locally to a zone
 *  Example : if (zone:getLocalVar("[POP]King_Arthro") > 0) then
 *  Notes   :
 ************************************************************************/

auto CLuaZone::getLocalVar(const char* key)
{
    return m_pLuaZone->GetLocalVar(key);
}

/************************************************************************
 *  Function: getLocalVars()
 *  Purpose : Returns all local variables assigned to the zone as a table
 *  Example : local vars = zone:getLocalVars()
 *  Notes   :
 ************************************************************************/
auto CLuaZone::getLocalVars() -> sol::table
{
    auto  table     = lua.create_table();
    auto& localVars = m_pLuaZone->GetLocalVars();

    for (const auto& [varName, value] : localVars)
    {
        auto subtable       = lua.create_table();
        subtable["varname"] = varName;
        subtable["value"]   = value;
        table.add(subtable);
    }
    return table;
}

/************************************************************************
 *  Function: setLocalVar()
 *  Purpose : Assigns a local variable to a zone
 *  Example : zone:setLocalVar("pop", GetSystemTime() + math.randomInt(1200, 7200));
 *  Notes   :
 ************************************************************************/

void CLuaZone::setLocalVar(const char* key, uint32 val)
{
    m_pLuaZone->SetLocalVar(key, val);
}

void CLuaZone::resetLocalVars()
{
    m_pLuaZone->ResetLocalVars();
}

/************************************************************************
 *                                                                       *
 * Registering the active trigger area in the zone                       *
 * Input data format: RegionID, x1, y1, z1, x2, y2, z2                   *
 *                                                                       *
 ************************************************************************/

void CLuaZone::registerCuboidTriggerArea(uint32 triggerAreaID, float xMin, float yMin, float zMin, float xMax, float yMax, float zMax, sol::optional<float> rotation)
{
    auto tArea = std::make_unique<CCuboidTriggerArea>(triggerAreaID, xMin, yMin, zMin, xMax, yMax, zMax, rotation.value_or(0.0f));
    m_pLuaZone->InsertTriggerArea(std::move(tArea));
}

void CLuaZone::registerCylindricalTriggerArea(uint32 triggerAreaID, float xPos, float zPos, float radius)
{
    auto tArea = std::make_unique<CCylindricalTriggerArea>(triggerAreaID, xPos, zPos, radius);
    m_pLuaZone->InsertTriggerArea(std::move(tArea));
}

void CLuaZone::registerSphericalTriggerArea(uint32 triggerAreaID, float xPos, float yPos, float zPos, float radius)
{
    auto tArea = std::make_unique<CSphericalTriggerArea>(triggerAreaID, xPos, yPos, zPos, radius);
    m_pLuaZone->InsertTriggerArea(std::move(tArea));
}

/************************************************************************
 *                                                                       *
 *  Setting the level limit for the zone                                 *
 *                                                                       *
 ************************************************************************/

auto CLuaZone::levelRestriction() -> sol::object
{
    return sol::lua_nil;
}

auto CLuaZone::getPlayers() -> sol::table
{
    auto table = lua.create_table();
    // clang-format off
    m_pLuaZone->ForEachChar([&table](CCharEntity* PChar)
    {
        table.add(PChar);
    });
    // clang-format on
    return table;
}

auto CLuaZone::getNPCs() -> sol::table
{
    auto table = lua.create_table();
    // clang-format off
    m_pLuaZone->ForEachNpc([&table](CNpcEntity* PNpc)
    {
        table.add(CLuaBaseEntity(PNpc));
    });
    // clang-format on
    return table;
}

auto CLuaZone::getMobs() -> sol::table
{
    auto table = lua.create_table();
    // clang-format off
    m_pLuaZone->ForEachMob([&table](CMobEntity* PMob)
    {
        table.add(CLuaBaseEntity(PMob));
    });
    // clang-format on
    return table;
}

auto CLuaZone::getID() -> xi::ZoneId
{
    return m_pLuaZone->GetID();
}

auto CLuaZone::getName() -> const std::string&
{
    return m_pLuaZone->getName();
}

auto CLuaZone::getRegionID() -> xi::Region
{
    return m_pLuaZone->GetRegionID();
}

auto CLuaZone::getTypeMask() -> xi::ZoneType
{
    return m_pLuaZone->GetTypeMask();
}

auto CLuaZone::getBattlefieldByInitiator(uint32 charID) -> CBattlefield*
{
    if (m_pLuaZone->battlefieldHandler())
    {
        return m_pLuaZone->battlefieldHandler()->GetBattlefieldByInitiator(charID);
    }
    return nullptr;
}

auto CLuaZone::getWeather() const -> xi::Weather
{
    return m_pLuaZone->weather().current();
}

auto CLuaZone::getUptime() -> uint32
{
    long long uptime = timer::count_seconds(timer::get_uptime());
    // returns the zone up time in seconds
    return static_cast<uint32>(uptime);
}

void CLuaZone::reloadNavmesh()
{
    m_pLuaZone->RebuildNavMesh();
}

void CLuaZone::rebuildNavmesh(const sol::table& table)
{
    NavMeshConfig config;

    config.cellSize                     = table.get_or("cellSize", config.cellSize);
    config.cellHeight                   = table.get_or("cellHeight", config.cellHeight);
    config.walkableSlopeAngle           = table.get_or("walkableSlopeAngle", config.walkableSlopeAngle);
    config.agentHeight                  = table.get_or("agentHeight", config.agentHeight);
    config.agentRadius                  = table.get_or("agentRadius", config.agentRadius);
    config.agentMaxClimb                = table.get_or("agentMaxClimb", config.agentMaxClimb);
    config.maxEdgeLen                   = table.get_or("maxEdgeLen", config.maxEdgeLen);
    config.maxSimplificationError       = table.get_or("maxSimplificationError", config.maxSimplificationError);
    config.minRegionArea                = table.get_or("minRegionArea", config.minRegionArea);
    config.mergeRegionArea              = table.get_or("mergeRegionArea", config.mergeRegionArea);
    config.maxVertsPerPoly              = table.get_or("maxVertsPerPoly", config.maxVertsPerPoly);
    config.detailSampleDist             = table.get_or("detailSampleDist", config.detailSampleDist);
    config.detailSampleMaxError         = table.get_or("detailSampleMaxError", config.detailSampleMaxError);
    config.tileSize                     = table.get_or("tileSize", config.tileSize);
    config.filterLowHangingObstacles    = table.get_or("filterLowHangingObstacles", config.filterLowHangingObstacles);
    config.filterLedgeSpans             = table.get_or("filterLedgeSpans", config.filterLedgeSpans);
    config.filterWalkableLowHeightSpans = table.get_or("filterWalkableLowHeightSpans", config.filterWalkableLowHeightSpans);
    config.generateOffMeshLinks         = table.get_or("generateOffMeshLinks", config.generateOffMeshLinks);
    config.offMeshMaxDrop               = table.get_or("offMeshMaxDrop", config.offMeshMaxDrop);
    config.offMeshHorizReach            = table.get_or("offMeshHorizReach", config.offMeshHorizReach);

    if (const auto ySkipPlanes = table.get<sol::optional<sol::table>>("ySkipPlanes"))
    {
        for (const auto& [_, value] : *ySkipPlanes)
        {
            config.ySkipPlanes.push_back(value.as<float>());
        }
    }

    if (const auto skipSpheres = table.get<sol::optional<sol::table>>("skipSpheres"))
    {
        for (const auto& [_, value] : *skipSpheres)
        {
            const auto sphere = value.as<sol::table>();

            config.skipSpheres.push_back(NavMeshSkipSphere{
                .center = {
                    sphere.get_or("x", 0.0f),
                    sphere.get_or("y", 0.0f),
                    sphere.get_or("z", 0.0f),
                },
                .radius = sphere.get_or("radius", 0.0f),
            });
        }
    }

    m_pLuaZone->RebuildNavMesh(config);
}

auto CLuaZone::isNavigablePoint(const sol::table& point) -> bool
{
    // clang-format off
    position_t position
    {
        point["x"].get_or<float>(0),
        point["y"].get_or<float>(0),
        point["z"].get_or<float>(0),
        point["moving"].get_or<uint16>(0),
        point["rot"].get_or<uint8>(0)
    };
    // clang-format on

    return m_pLuaZone->navMesh()->validPosition(position);
}

auto CLuaZone::insertDynamicEntity(sol::table table) -> CBaseEntity*
{
    return luautils::GenerateDynamicEntity(m_pLuaZone, nullptr, std::move(table));
}

/************************************************************************
 *  Function: spawnBot({ name = 'Name', race = 1, job = 1, level = 10, x = 0, y = 0, z = 0, ... })
 *  Purpose : Puts a memory-only player bot in the zone (needs map.PLAYER_BOTS_ENABLED)
 ************************************************************************/

auto CLuaZone::spawnBot(const sol::table& table) -> CCharEntity*
{
    botutils::BotSpec spec;
    spec.name         = table.get_or<std::string>("name", "");
    spec.race         = table.get_or<uint8>("race", 1);
    spec.face         = table.get_or<uint8>("face", 0);
    spec.size         = table.get_or<uint8>("size", 0);
    spec.mainJob      = table.get_or<uint8>("job", 1);
    spec.mainLevel    = table.get_or<uint8>("level", 1);
    spec.subJob       = table.get_or<uint8>("subJob", 0);
    spec.subLevel     = table.get_or<uint8>("subLevel", 0);
    spec.pos.x        = table.get_or<float>("x", 0);
    spec.pos.y        = table.get_or<float>("y", 0);
    spec.pos.z        = table.get_or<float>("z", 0);
    spec.pos.rotation = table.get_or<uint8>("rot", 0);
    spec.seeking      = table.get_or("seeking", false);

    if (const auto gear = table.get<sol::optional<sol::table>>("gear"))
    {
        for (const auto& [key, value] : gear.value())
        {
            spec.gear.emplace_back(key.as<uint8>(), value.as<uint16>());
        }
    }

    if (spec.name.empty())
    {
        ShowWarning("CLuaZone::spawnBot: a bot needs a name");
        return nullptr;
    }

    return botutils::SpawnBot(m_pLuaZone, spec);
}

void CLuaZone::despawnBots()
{
    m_pLuaZone->DespawnBots();
}

auto CLuaZone::getBotCount() -> uint32
{
    return static_cast<uint32>(m_pLuaZone->GetBotCount());
}

auto CLuaZone::getBotEntrances() -> sol::table
{
    auto table = lua.create_table();
    for (const auto& pos : botutils::GetEntrances(m_pLuaZone))
    {
        table.add(lua.create_table_with("x", pos.x, "y", pos.y, "z", pos.z, "rot", pos.rotation));
    }

    return table;
}

auto CLuaZone::getBotScatterPoint() -> sol::optional<sol::table>
{
    if (const auto point = botutils::ScatterPoint(m_pLuaZone))
    {
        return lua.create_table_with("x", point->x, "y", point->y, "z", point->z, "rot", point->rotation);
    }

    return sol::nullopt;
}

auto CLuaZone::getGroundHeight(const float x, const float top, const float z, const float bottom) -> sol::optional<float>
{
    const position_t pos{ x, top, z, 0, 0 };
    if (const auto ground = botutils::GroundBelow(m_pLuaZone, pos, top, bottom))
    {
        return *ground;
    }

    return sol::nullopt;
}

auto CLuaZone::getNavMeshHeight(const float x, const float y, const float z) -> sol::optional<float>
{
    // The navmesh floor nearest the point, if one lies straight under or over it.
    position_t pos{ x, y, z, 0, 0 };
    m_pLuaZone->navMesh()->snapToValidPosition(pos);
    if (::distance(pos, position_t{ x, y, z, 0, 0 }, true) > 0.1f)
    {
        return sol::nullopt;
    }

    return pos.y;
}

auto CLuaZone::getFloorHeight(const float x, const float from, const float z, const float to) -> sol::optional<float>
{
    if (const auto floor = botutils::FloorAlong(m_pLuaZone, position_t{ x, from, z, 0, 0 }, from, to))
    {
        return *floor;
    }

    return sol::nullopt;
}

auto CLuaZone::getBots() -> sol::table
{
    auto table = lua.create_table();
    m_pLuaZone->ForEachBot(
        [&table](CCharEntity* PBot)
        {
            table.add(CLuaBaseEntity(PBot));
        });

    return table;
}

/************************************************************************
 *  Function: SetSoloBattleMusic(253)
 *  Purpose : Set Solo Battle music for zone
 ************************************************************************/

void CLuaZone::setSoloBattleMusic(uint16 musicId)
{
    m_pLuaZone->SetSoloBattleMusic(musicId);
}

auto CLuaZone::getSoloBattleMusic()
{
    return m_pLuaZone->GetSoloBattleMusic();
}

/************************************************************************
 *  Function: SetPartyBattleMusic(253)
 *  Purpose : Set Party Battle music for zone
 ************************************************************************/

void CLuaZone::setPartyBattleMusic(uint16 musicId)
{
    m_pLuaZone->SetPartyBattleMusic(musicId);
}

auto CLuaZone::getPartyBattleMusic()
{
    return m_pLuaZone->GetPartyBattleMusic();
}

/************************************************************************
 *  Function: SetBackgroundMusicDay(253)
 *  Purpose : Set Background Day music for zone
 ************************************************************************/

void CLuaZone::setBackgroundMusicDay(uint16 musicId)
{
    m_pLuaZone->SetBackgroundMusicDay(musicId);
}

auto CLuaZone::getBackgroundMusicDay()
{
    return m_pLuaZone->GetBackgroundMusicDay();
}

/************************************************************************
 *  Function: SetBackgroundMusicNight(253)
 *  Purpose : Set Background Night music for zone
 ************************************************************************/

void CLuaZone::setBackgroundMusicNight(uint16 musicId)
{
    m_pLuaZone->SetBackgroundMusicNight(musicId);
}

auto CLuaZone::getBackgroundMusicNight()
{
    return m_pLuaZone->GetBackgroundMusicNight();
}

auto CLuaZone::queryEntitiesByName(const std::string& name) -> sol::table
{
    const QueryByNameResult_t& entities = m_pLuaZone->queryEntitiesByName(name);

    auto table = lua.create_table();
    for (CBaseEntity* entity : entities)
    {
        table.add(CLuaBaseEntity(entity));
    }

    if (table.empty())
    {
        ShowWarning("Query for entity name: %s in zone: %s returned no results", name, m_pLuaZone->getName());
    }

    return table;
}

/************************************************************************
 *  Function: getTerrainType()
 *  Purpose : Returns the terrain type at the given position
 *  Example : zone:getTerrainType(player:getPos())
 ************************************************************************/

auto CLuaZone::getTerrainType(const sol::table& position) -> xi::TerrainType
{
    return m_pLuaZone->xiMesh()->getTerrainAt(
        position["x"].get_or<float>(0),
        position["y"].get_or<float>(0),
        position["z"].get_or<float>(0));
}

/************************************************************************
 *  Function: getFloorId()
 *  Purpose : Returns the floor ID at the given position
 *  Example : zone:getFloorId(player:getPos())
 ************************************************************************/

auto CLuaZone::getFloorId(const sol::table& position) -> uint8
{
    return m_pLuaZone->xiMesh()->getFloorId(
        position["x"].get_or<float>(0),
        position["y"].get_or<float>(0),
        position["z"].get_or<float>(0));
}

//======================================================//

void CLuaZone::Register()
{
    SOL_USERTYPE("CZone", CLuaZone);

    SOL_REGISTER("getLocalVar", CLuaZone::getLocalVar);
    SOL_REGISTER("getLocalVars", CLuaZone::getLocalVars);
    SOL_REGISTER("setLocalVar", CLuaZone::setLocalVar);
    SOL_REGISTER("resetLocalVars", CLuaZone::resetLocalVars);

    SOL_REGISTER("registerCuboidTriggerArea", CLuaZone::registerCuboidTriggerArea);
    SOL_REGISTER("registerCylindricalTriggerArea", CLuaZone::registerCylindricalTriggerArea);
    SOL_REGISTER("registerSphericalTriggerArea", CLuaZone::registerSphericalTriggerArea);

    SOL_REGISTER("levelRestriction", CLuaZone::levelRestriction);
    SOL_REGISTER("getPlayers", CLuaZone::getPlayers);
    SOL_REGISTER("getNPCs", CLuaZone::getNPCs);
    SOL_REGISTER("getMobs", CLuaZone::getMobs);
    SOL_REGISTER("getID", CLuaZone::getID);
    SOL_REGISTER("getName", CLuaZone::getName);
    SOL_REGISTER("getRegionID", CLuaZone::getRegionID);
    SOL_REGISTER("getTypeMask", CLuaZone::getTypeMask);
    SOL_REGISTER("getBattlefieldByInitiator", CLuaZone::getBattlefieldByInitiator);
    SOL_REGISTER("getWeather", CLuaZone::getWeather);
    SOL_REGISTER("getUptime", CLuaZone::getUptime);
    SOL_REGISTER("reloadNavmesh", CLuaZone::reloadNavmesh);
    SOL_REGISTER("rebuildNavmesh", CLuaZone::rebuildNavmesh);
    SOL_REGISTER("isNavigablePoint", CLuaZone::isNavigablePoint);
    SOL_REGISTER("getTerrainType", CLuaZone::getTerrainType);
    SOL_REGISTER("getFloorId", CLuaZone::getFloorId);
    SOL_REGISTER("insertDynamicEntity", CLuaZone::insertDynamicEntity);
    SOL_REGISTER("spawnBot", CLuaZone::spawnBot);
    SOL_REGISTER("despawnBots", CLuaZone::despawnBots);
    SOL_REGISTER("getBotCount", CLuaZone::getBotCount);
    SOL_REGISTER("getBots", CLuaZone::getBots);
    SOL_REGISTER("getBotEntrances", CLuaZone::getBotEntrances);
    SOL_REGISTER("getBotScatterPoint", CLuaZone::getBotScatterPoint);
    SOL_REGISTER("getGroundHeight", CLuaZone::getGroundHeight);
    SOL_REGISTER("getNavMeshHeight", CLuaZone::getNavMeshHeight);
    SOL_REGISTER("getFloorHeight", CLuaZone::getFloorHeight);

    SOL_REGISTER("getSoloBattleMusic", CLuaZone::getSoloBattleMusic);
    SOL_REGISTER("getPartyBattleMusic", CLuaZone::getPartyBattleMusic);
    SOL_REGISTER("getBackgroundMusicDay", CLuaZone::getBackgroundMusicDay);
    SOL_REGISTER("getBackgroundMusicNight", CLuaZone::getBackgroundMusicNight);
    SOL_REGISTER("setSoloBattleMusic", CLuaZone::setSoloBattleMusic);
    SOL_REGISTER("setPartyBattleMusic", CLuaZone::setPartyBattleMusic);
    SOL_REGISTER("setBackgroundMusicDay", CLuaZone::setBackgroundMusicDay);
    SOL_REGISTER("setBackgroundMusicNight", CLuaZone::setBackgroundMusicNight);

    SOL_REGISTER("queryEntitiesByName", CLuaZone::queryEntitiesByName);
}

auto operator<<(std::ostream& os, const CLuaZone& zone) -> std::ostream&
{
    std::string id = zone.m_pLuaZone ? std::to_string(static_cast<uint16>(zone.m_pLuaZone->GetID())) : "nullptr";
    return os << "CLuaZone(" << id << ")";
}

//======================================================//
