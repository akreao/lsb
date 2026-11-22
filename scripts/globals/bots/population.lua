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

-- Bots a zone aims for, by kind of zone (settings). Each zone draws its place in the range once, so its count stays
-- steady and follows the setting when it changes.
local function targetRanges()
    local settings = xi.settings.map
    return
    {
        { xi.zoneType.CITY,     settings.PLAYER_BOTS_CITY_COUNT or { 8, 14 } },
        { xi.zoneType.OUTDOORS, settings.PLAYER_BOTS_OUTDOORS_COUNT or { 8, 14 } },
        { xi.zoneType.DUNGEON,  settings.PLAYER_BOTS_DUNGEON_COUNT or { 6, 10 } },
    }
end

local zoneTargets = {}

-- Bots never arrive at a point a player is closer to than this.
local arrivalClearRange = 5

-- A zone nobody has visited for a while gets this many bots at once, spread across it, as if they had been
-- there all along; the rest trickle in through the entrances.
local firstWave = 3

-- Regulars: some bots that log out are remembered (name, looks, job, level, gear) and come back later, so a
-- server has familiar faces. Memory only: a map restart forgets them.
local maxRegulars    = 8
local keepOdds       = 3 -- one bot in this many is kept as a regular when it logs out
local regularOdds    = 4 -- one arrival in this many is a regular, when one fits the zone
local regulars       = {}

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
    zoneTargets[zoneId] = zoneTargets[zoneId] or math.randomFloat(0, 1)

    local mask   = zone:getTypeMask()
    local target = 0
    for _, range in ipairs(targetRanges()) do
        if hasFlag(mask, range[1]) then
            local low  = range[2][1] or 0
            local high = math.max(range[2][2] or low, low)
            target     = math.min(low + math.floor(zoneTargets[zoneId] * (high - low + 1)), high)
            break
        end
    end

    return math.min(target, xi.settings.map.PLAYER_BOTS_MAX_PER_ZONE or 20)
end

-- Real players a zone counts for bots: GM-hidden ones do not.
xi.bots.realPlayers = function(zone)
    local players = {}
    for _, player in ipairs(zone:getPlayers()) do
        if not player:getGMHidden() then
            table.insert(players, player)
        end
    end

    return players
end

-- A random arrival point, as if the bot had just zoned in there; never closer than arrivalClearRange to a real
-- player, so a bot does not appear on top of someone.
local function pickArrival(zone)
    local players = xi.bots.realPlayers(zone)
    local choices = {}

    for _, pos in ipairs(zone:getBotEntrances()) do
        local clear = true
        for _, player in ipairs(players) do
            local there = { x = player:getXPos(), y = player:getYPos(), z = player:getZPos() }
            if utils.distance(pos, there) < arrivalClearRange then
                clear = false
                break
            end
        end

        if clear then
            table.insert(choices, pos)
        end
    end

    if #choices == 0 then
        return nil
    end

    return choices[math.random(1, #choices)]
end

-- A regular who fits the zone: any level in a city, the zone's band (and a few levels above) elsewhere.
local function pickRegular(zone)
    if #regulars == 0 or math.random(1, regularOdds) ~= 1 then
        return nil
    end

    local low, high = xi.bots.zoneLevelBand(zone)
    local choices   = {}
    for _, regular in ipairs(regulars) do
        if not low or (regular.level >= low and regular.level <= high + 4) then
            table.insert(choices, regular)
        end
    end

    if #choices == 0 then
        return nil
    end

    return choices[math.random(1, #choices)]
end

local function arrive(zone, pos)
    local regular = pickRegular(zone)
    if regular then
        local overrides = {}
        for key, value in pairs(regular) do
            overrides[key] = value
        end

        -- Nil when the regular is already online somewhere: a stranger comes instead.
        local bot = xi.bots.spawn(zone, pos, overrides)
        if bot then
            return bot
        end
    end

    return xi.bots.spawn(zone, pos)
end

xi.bots.populate = function(zone)
    if
        not xi.settings.map.PLAYER_BOTS_ENABLED or
        not xi.bots.isZoneAllowed(zone)
    then
        return
    end

    local target = xi.bots.targetCount(zone)
    if zone:getBotCount() >= target then
        return
    end

    if zone:getBotCount() == 0 then
        local made = 0
        for _ = 1, math.min(firstWave, target) do
            local pos = zone:getBotScatterPoint()
            if pos and arrive(zone, pos) then
                made = made + 1
            end
        end

        if made > 0 then
            return
        end
    end

    local pos = pickArrival(zone)
    if pos then
        arrive(zone, pos)
    end
end

-- Called by the map when a bot's play session ends and it logs out: maybe keep it as a regular.
xi.bots.onLogout = function(bot)
    for i, regular in ipairs(regulars) do
        if regular.name == bot:getName() then
            table.remove(regulars, i)
            break
        end
    end

    if #regulars >= maxRegulars or math.random(1, keepOdds) ~= 1 then
        return
    end

    local gear = {}
    for slot = xi.slot.MAIN, xi.slot.BACK do
        local itemId = bot:getEquipID(slot)
        if itemId and itemId > 0 then
            gear[slot] = itemId
        end
    end

    table.insert(regulars,
    {
        name     = bot:getName(),
        job      = bot:getMainJob(),
        level    = bot:getMainLvl(),
        subJob   = bot:getSubJob(),
        subLevel = bot:getSubLvl(),
        race     = bot:getRace(),
        face     = bot:getFace(),
        size     = bot:getSize(),
        gear     = gear,
    })
end

-- The regulars remembered so far (tests and `!bot` read it).
xi.bots.getRegulars = function()
    return regulars
end

-- Forgets per-zone picks and chat cooldowns (map ResetBots calls it; tests run it between cases).
xi.bots.reset = function()
    zoneTargets = {}
    regulars    = {}
    if xi.bots.resetChat then
        xi.bots.resetChat()
    end
end
