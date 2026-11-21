-----------------------------------
-- Player bots: which zones get bots, how many, and where they arrive (custom, not retail)
-- The map calls xi.bots.populate(zone) every map.PLAYER_BOTS_ARRIVAL_INTERVAL seconds while the zone has a
-- real player; each call adds at most one bot, so bots trickle in. Bots leave on their own a while after the
-- last real player goes (map.PLAYER_BOTS_LEAVE_DELAY).
-----------------------------------
require('scripts/globals/bots/builder')
-----------------------------------
xi = xi or {}
xi.bots = xi.bots or {}

-- Bots a zone aims for, by kind of zone; picked once per zone so the count stays steady.
local targetRanges =
{
    { xi.zoneType.CITY,     8, 12 },
    { xi.zoneType.OUTDOORS, 6, 10 },
    { xi.zoneType.DUNGEON,  4,  8 },
}

local zoneTargets = {}

-- Bots never walk into an arrival point while a player is closer than this.
local arrivalClearRange = 20

local function hasFlag(mask, flag)
    return bit.band(mask, flag) ~= 0
end

-- Cities, outdoor zones and dungeons only; never instances, Dynamis or a zone on the excluded list.
xi.bots.isZoneAllowed = function(zone)
    local mask = zone:getTypeMask()
    if
        hasFlag(mask, xi.zoneType.INSTANCED) or
        hasFlag(mask, xi.zoneType.DYNAMIS)
    then
        return false
    end

    for _, zoneId in ipairs(xi.settings.map.PLAYER_BOTS_EXCLUDED_ZONES or {}) do
        if zone:getID() == zoneId then
            return false
        end
    end

    return hasFlag(mask, xi.zoneType.CITY) or hasFlag(mask, xi.zoneType.OUTDOORS) or hasFlag(mask, xi.zoneType.DUNGEON)
end

xi.bots.targetCount = function(zone)
    local zoneId = zone:getID()
    if not zoneTargets[zoneId] then
        local mask = zone:getTypeMask()
        local target = 0
        for _, range in ipairs(targetRanges) do
            if hasFlag(mask, range[1]) then
                target = math.random(range[2], range[3])
                break
            end
        end

        zoneTargets[zoneId] = target
    end

    return math.min(zoneTargets[zoneId], xi.settings.map.PLAYER_BOTS_MAX_PER_ZONE or 12)
end

-- An arrival point no real player is standing near, so a bot never pops up next to someone.
local function pickArrival(zone)
    local entrances = zone:getBotEntrances()
    local players   = zone:getPlayers()
    local clear     = {}

    for _, pos in ipairs(entrances) do
        local isClear = true
        for _, player in ipairs(players) do
            local there = { x = player:getXPos(), y = player:getYPos(), z = player:getZPos() }
            if utils.distance(pos, there) < arrivalClearRange then
                isClear = false
                break
            end
        end

        if isClear then
            table.insert(clear, pos)
        end
    end

    if #clear == 0 then
        return nil
    end

    return clear[math.random(1, #clear)]
end

xi.bots.populate = function(zone)
    if
        not xi.settings.map.PLAYER_BOTS_ENABLED or
        not xi.bots.isZoneAllowed(zone)
    then
        return
    end

    if zone:getBotCount() >= xi.bots.targetCount(zone) then
        return
    end

    local pos = pickArrival(zone)
    if pos then
        xi.bots.spawn(zone, pos)
    end
end
