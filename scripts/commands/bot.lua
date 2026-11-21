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

-- Level 7 leather set and a bronze sword, for a quick look at a bot.
local testGear =
{
    [xi.slot.MAIN]  = xi.item.BRONZE_SWORD,
    [xi.slot.SUB]   = xi.item.LAUAN_SHIELD,
    [xi.slot.HEAD]  = xi.item.LEATHER_BANDANA,
    [xi.slot.BODY]  = xi.item.LEATHER_VEST,
    [xi.slot.HANDS] = xi.item.LEATHER_GLOVES,
    [xi.slot.LEGS]  = xi.item.LEATHER_TROUSERS,
    [xi.slot.FEET]  = xi.item.LEATHER_HIGHBOOTS,
}

local function usage(player, msg)
    if msg then
        player:printToPlayer(msg)
    end

    player:printToPlayer('!bot spawn <name> (job id) (level) | !bot clear | !bot count')
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

    if action == 'spawn' then
        if not name then
            usage(player, 'A bot needs a name.')
            return
        end

        local bot = zone:spawnBot({
            name     = name,
            race     = xi.race.HUME_M,
            face     = 0,
            job      = tonumber(job) or xi.job.WAR,
            level    = tonumber(level) or 10,
            x        = player:getXPos() + 2,
            y        = player:getYPos(),
            z        = player:getZPos(),
            rot      = player:getRotPos(),
            gear     = testGear,
        })

        if bot then
            player:printToPlayer(string.format('Spawned bot %s (id %d).', bot:getName(), bot:getID()))
        else
            player:printToPlayer('Could not spawn a bot here.')
        end
    elseif action == 'clear' then
        zone:despawnBots()
        player:printToPlayer('Cleared all bots in this zone.')
    elseif action == 'count' then
        player:printToPlayer(string.format('Bots in this zone: %d', zone:getBotCount()))
    else
        usage(player)
    end
end

return commandObj
