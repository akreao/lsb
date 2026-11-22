-----------------------------------
-- func: bot
-- desc: GM tools for memory-only player bots (needs map.PLAYER_BOTS_ENABLED). GM level 3 looks, lists, clears,
--       pauses; GM level 5 also spawns and makes bots.
-----------------------------------
---@type TCommand
local commandObj = {}

commandObj.cmdprops =
{
    permission = 3,
    parameters = 'ssss'
}

local jobNames =
{
    'WAR', 'MNK', 'WHM', 'BLM', 'RDM', 'THF', 'PLD', 'DRK', 'BST', 'BRD', 'RNG',
    'SAM', 'NIN', 'DRG', 'SMN', 'BLU', 'COR', 'PUP', 'DNC', 'SCH', 'GEO', 'RUN',
}

-- GM level needed to put bots into the world by hand.
local makeLevel = 5

local function usage(player, msg)
    if msg then
        player:printToPlayer(msg)
    end

    player:printToPlayer('!bot list | count | all | whois <name> | clear | pause')
    if player:getGMLevel() >= makeLevel then
        player:printToPlayer('!bot spawn (count) | make <name> (job) (level)')
    end
end

local function formatTime(seconds)
    if seconds >= 3600 then
        return string.format('%dh%02dm', math.floor(seconds / 3600), math.floor(seconds % 3600 / 60))
    end

    return string.format('%dm', math.floor(seconds / 60))
end

local function describe(info)
    local support = info.subJob ~= xi.job.NONE and string.format('/%s%d', jobNames[info.subJob], info.subLevel) or ''
    local party   = info.partyWith ~= '' and string.format(', party of %s', info.partyWith) or ''
    return string.format('%s (id %d): %s%d%s in %s, %s%s, online %s', info.name, info.id, jobNames[info.job] or '?',
        info.level, support, info.zoneName, info.state, party, formatTime(info.onlineSeconds))
end

-- Actions that work anywhere (server-wide views and switches).
local global = {}

global.all = function(player)
    local counts = {}
    local names  = {}
    local total  = 0
    for _, info in ipairs(GetBotList()) do
        if not counts[info.zoneName] then
            counts[info.zoneName] = 0
            table.insert(names, info.zoneName)
        end

        counts[info.zoneName] = counts[info.zoneName] + 1
        total = total + 1
    end

    table.sort(names)
    player:printToPlayer(string.format('Bots on this server: %d (cap %d)%s', total,
        xi.settings.map.PLAYER_BOTS_MAX_TOTAL or 0, AreBotsPaused() and ', paused' or ''))
    for _, zoneName in ipairs(names) do
        player:printToPlayer(string.format('  %s: %d', zoneName, counts[zoneName]))
    end
end

global.whois = function(player, name)
    if not name then
        usage(player, 'Whois needs a name.')
        return
    end

    for _, info in ipairs(GetBotList()) do
        if string.lower(info.name) == string.lower(name) then
            player:printToPlayer(describe(info))
            return
        end
    end

    player:printToPlayer(string.format('No live bot is called %s.', name))
end

global.pause = function(player)
    SetBotsPaused(not AreBotsPaused())
    player:printToPlayer(AreBotsPaused() and 'Bots paused: no new bots arrive and none start a hunt.' or
        'Bots running again.')
end

-- Actions on the GM's zone; spawn and make need makeLevel.
local local_ = {}
local makers = { spawn = true, make = true }

local_.spawn = function(player, zone, here, count)
    -- Random bots that fit the zone, in a ring around the GM.
    count = utils.clamp(tonumber(count) or 1, 1, 12)
    local made = 0
    for i = 1, count do
        local angle = (i / count) * 2 * math.pi
        local pos   = { x = here.x + 3 * math.cos(angle), y = here.y, z = here.z + 3 * math.sin(angle) }
        local bot   = xi.bots.spawn(zone, pos)
        if bot then
            made = made + 1
            local support = bot:getSubJob() ~= xi.job.NONE and
                string.format('/%s%d', jobNames[bot:getSubJob()], bot:getSubLvl()) or ''
            player:printToPlayer(string.format('Spawned %s: %s%d%s.', bot:getName(),
                jobNames[bot:getMainJob()], bot:getMainLvl(), support))
        end
    end

    if made == 0 then
        player:printToPlayer('Could not spawn a bot here.')
    end
end

local_.make = function(player, zone, here, name, job, level)
    if not name then
        usage(player, 'A bot needs a name.')
        return
    end

    local jobId = tonumber(job) or (job and xi.job[string.upper(job)])
    local bot   = xi.bots.spawn(zone, { x = here.x + 2, y = here.y, z = here.z, rot = here.rot },
    {
        name  = name,
        job   = jobId,
        level = tonumber(level),
    })

    if bot then
        player:printToPlayer(string.format('Spawned bot %s (id %d).', bot:getName(), bot:getID()))
    else
        player:printToPlayer('Could not spawn a bot here (the name may be taken).')
    end
end

local_.clear = function(player, zone)
    zone:despawnBots()
    player:printToPlayer('Cleared all bots in this zone.')
end

local_.count = function(player, zone)
    player:printToPlayer(string.format('Bots in this zone: %d', zone:getBotCount()))
end

local_.list = function(player, zone, here)
    local bots = zone:getBots()
    player:printToPlayer(string.format('Bots in this zone: %d', #bots))
    for _, bot in ipairs(bots) do
        local there = { x = bot:getXPos(), y = bot:getYPos(), z = bot:getZPos() }
        player:printToPlayer(string.format('%s %s%d: %d yalms away at (%d, %d, %d)%s%s', bot:getName(),
            jobNames[bot:getMainJob()], bot:getMainLvl(), math.floor(utils.distance(here, there)),
            math.floor(there.x), math.floor(there.y), math.floor(there.z),
            bot:isEngaged() and ', fighting' or '', bot:getPartySize() > 1 and ', in a party' or ''))
    end
end

commandObj.onTrigger = function(player, action, name, job, level)
    if not xi.settings.map.PLAYER_BOTS_ENABLED then
        player:printToPlayer('Player bots are off (map.PLAYER_BOTS_ENABLED).')
        return
    end

    if global[action] then
        global[action](player, name)
        return
    end

    if not local_[action] then
        usage(player)
        return
    end

    if makers[action] and player:getGMLevel() < makeLevel then
        usage(player, string.format('Spawning bots needs GM level %d.', makeLevel))
        return
    end

    local zone = player:getZone()
    if not zone or player:getInstance() then
        usage(player, 'Bots cannot be used here.')
        return
    end

    local here = { x = player:getXPos(), y = player:getYPos(), z = player:getZPos(), rot = player:getRotPos() }
    local_[action](player, zone, here, name, job, level)
end

return commandObj
