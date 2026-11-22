-----------------------------------
-- func: bots
-- desc: Lets any player turn the server's AI adventurers (player bots) off or on for themselves.
--       Off: bots are not drawn for them, never talk to them, and never hunt near them (custom, not retail).
-----------------------------------
---@type TCommand
local commandObj = {}

commandObj.cmdprops =
{
    permission = 0,
    parameters = 's'
}

-- Read by the map (botutils::OptOutVar); the website toggle writes the same char var.
local optOutVar = 'BotsOptOut'

commandObj.onTrigger = function(player, choice)
    choice = choice and string.lower(choice) or ''

    if choice == 'off' then
        player:setCharVar(optOutVar, 1)
        player:printToPlayer('AI adventurers are now hidden for you. Type !bots on to see them again.')
    elseif choice == 'on' then
        player:setCharVar(optOutVar, 0)
        player:printToPlayer('AI adventurers are shown again.')
    else
        local state = player:getCharVar(optOutVar) > 0 and 'off' or 'on'
        player:printToPlayer(string.format('AI adventurers are %s for you. Use !bots off or !bots on.', state))
    end
end

return commandObj
