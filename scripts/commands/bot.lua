-----------------------------------
-- func: bot
-- desc: Spawns or clears memory-only player bots in the current zone (needs map.PLAYER_BOTS_ENABLED).
-----------------------------------
---@type TCommand
local commandObj = {}

commandObj.cmdprops =
{
    permission = 5,
    parameters = 'ssss'
}

local jobNames =
{
    'WAR', 'MNK', 'WHM', 'BLM', 'RDM', 'THF', 'PLD', 'DRK', 'BST', 'BRD', 'RNG',
    'SAM', 'NIN', 'DRG', 'SMN', 'BLU', 'COR', 'PUP', 'DNC', 'SCH', 'GEO', 'RUN',
}

local function usage(player, msg)
    if msg then
        player:printToPlayer(msg)
    end

    player:printToPlayer('!bot spawn (count) | !bot make <name> (job) (level) | !bot clear | !bot count | !bot list')
end

commandObj.onTrigger = function(player, action, name, job, level)
    if not xi.settings.map.PLAYER_BOTS_ENABLED then
        player:printToPlayer('Player bots are off (map.PLAYER_BOTS_ENABLED).')
        return
    end

    local zone = player:getZone()
    if not zone or player:getInstance() then
        usage(player, 'Bots cannot be used here.')
        return
    end

    local here = { x = player:getXPos(), y = player:getYPos(), z = player:getZPos(), rot = player:getRotPos() }

    if action == 'spawn' then
        -- Random bots that fit the zone, in a ring around the GM.
        local count = utils.clamp(tonumber(name) or 1, 1, 12)
        local made  = 0
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
    elseif action == 'make' then
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
    elseif action == 'clear' then
        zone:despawnBots()
        player:printToPlayer('Cleared all bots in this zone.')
    elseif action == 'count' then
        player:printToPlayer(string.format('Bots in this zone: %d', zone:getBotCount()))
    elseif action == 'list' then
        local bots = zone:getBots()
        player:printToPlayer(string.format('Bots in this zone: %d', #bots))
        for _, bot in ipairs(bots) do
            local there = { x = bot:getXPos(), y = bot:getYPos(), z = bot:getZPos() }
            player:printToPlayer(string.format('%s %s%d: %d yalms away at (%d, %d, %d)%s%s', bot:getName(),
                jobNames[bot:getMainJob()], bot:getMainLvl(), math.floor(utils.distance(here, there)),
                math.floor(there.x), math.floor(there.y), math.floor(there.z),
                bot:isEngaged() and ', fighting' or '', bot:getPartySize() > 1 and ', in a party' or ''))
        end
    else
        usage(player)
    end
end

return commandObj
