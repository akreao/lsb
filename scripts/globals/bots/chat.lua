-----------------------------------
-- Player bots: the few canned lines they say (custom, not retail)
-- The map asks xi.bots.chatLine(bot, event) when something happens and sends what it returns as a tell,
-- party or say line. Returning nil says nothing. Lines are fixed text, never generated, and cooldowns
-- keep the chatter rare. map.PLAYER_BOTS_CHAT turns all of it off.
-----------------------------------
xi = xi or {}
xi.bots = xi.bots or {}

local jobNames =
{
    [xi.job.WAR] = 'WAR', [xi.job.MNK] = 'MNK', [xi.job.WHM] = 'WHM', [xi.job.BLM] = 'BLM',
    [xi.job.RDM] = 'RDM', [xi.job.THF] = 'THF', [xi.job.PLD] = 'PLD', [xi.job.DRK] = 'DRK',
    [xi.job.BST] = 'BST', [xi.job.BRD] = 'BRD', [xi.job.RNG] = 'RNG', [xi.job.SAM] = 'SAM',
    [xi.job.NIN] = 'NIN', [xi.job.DRG] = 'DRG', [xi.job.SMN] = 'SMN', [xi.job.BLU] = 'BLU',
    [xi.job.COR] = 'COR', [xi.job.PUP] = 'PUP', [xi.job.DNC] = 'DNC', [xi.job.SCH] = 'SCH',
    [xi.job.GEO] = 'GEO', [xi.job.RUN] = 'RUN',
}

-- Lines per event. A line may be a function of the bot, for lines that name its job or level.
local lines =
{
    refuse_high =
    {
        'Sorry, you\'re too high level for me.',
        'Thanks, but you\'re a bit high for me. I\'d just slow you down.',
    },

    refuse_low =
    {
        'Sorry, you\'re a bit low for me.',
        'Thanks, but we\'re too far apart in level.',
    },

    refuse_far =
    {
        function(bot)
            return string.format('Thanks! I\'m out in %s though. Come find me there!', (bot:getZoneName():gsub('_', ' ')))
        end,
    },

    refuse_busy =
    {
        'Sorry, I\'m already in a party.',
    },

    refuse_full =
    {
        'Looks like your party is full.',
    },

    refuse_bots =
    {
        'Looks like you\'ve got plenty of help already.',
        'Sorry, I\'d rather party with a few more people.',
    },

    refuse_seeker =
    {
        'There\'s someone here looking for a party, ask them first!',
        'Sorry, I think someone nearby is looking for a group. Try them?',
    },

    refuse_alliance =
    {
        'Sorry, I don\'t do alliances.',
    },

    join =
    {
        'Hi all!',
        'Hello! Ready when you are.',
        'Thanks for the invite!',
        function(bot)
            return string.format('Hi! %s%d here.', jobNames[bot:getMainJob()] or '', bot:getMainLvl())
        end,
    },

    leave =
    {
        'Thanks for the party!',
        'Good party, see you around.',
        'Thanks all, take care.',
    },

    dismissed =
    {
        'No problem, good luck!',
        'Thanks for the party!',
    },

    death =
    {
        'Oops...',
        'Sorry about that.',
        'Could use a raise if anyone has one.',
    },

    -- Tell replies depend on what the bot is doing (map: botutils state).
    tell =
    {
        'Sorry, a bit busy at the moment.',
        'Hi! Can\'t talk much right now.',
    },

    tell_hunting =
    {
        'Sorry, busy leveling right now.',
        'Hi! Can\'t talk much, I\'m fighting.',
        'Mid-fight, sorry!',
    },

    tell_party =
    {
        'In a party right now, sorry.',
        'Can\'t chat, my party is pulling.',
    },

    tell_seeking =
    {
        'Hi! Looking for a party, send me an invite!',
        function(bot)
            return string.format('%s%d looking for a party, send me an invite.', jobNames[bot:getMainJob()] or '', bot:getMainLvl())
        end,
    },

    tell_town =
    {
        'Hi! Just taking a break in town.',
        'Hey, just resting up before heading out.',
        'Hi! Sorting my things, what\'s up?',
    },

    idle_city =
    {
        function(bot)
            return string.format('%s%d LFP', jobNames[bot:getMainJob()] or '', bot:getMainLvl())
        end,

        'Anyone want to party?',
        'Back from camping, time for a break.',
        'Anyone heading out to level?',
        'Nice day for it.',
        'Anyone know where to buy arrows around here?',
        'Phew, that last camp was rough.',
        'Need to repair my gear... oh wait.',
        'Does anyone have a spare Echo Drops?',
        'I keep forgetting to vote for the conquest.',
        'The weather looks good for fishing.',
        'Selling? No, just looking around.',
        'Ugh, I died twice to that crab.',
        'Gotta save up for my next weapon.',
        'Has anyone seen the airship schedule?',
        'My chocobo license test went well!',
        'Brb, grabbing food.',
        'Who\'s up for a run to the dunes?',
        'These boots are killing me.',
        'I should really level my support job.',
        'Is the auction house busy today?',
        'Need a few more levels before the next mission.',
        'So many quests, so little time.',
        'Anyone else get lost in the city every time?',
        'My moogle keeps losing my letters.',
        'Just got a new spell, can\'t wait to try it.',
        'That was a good party last night.',
        'Time to sort my inventory again.',
        'Waiting on a friend.',
        'Heading out soon, anyone going my way?',
        'I could use some crystals for crafting.',
        'Morning, everyone.',
        'Back to the grind after this.',
        'Finally got that drop!',
    },
}

-- Seconds before the same bot says a line for the event again. Events not listed always speak, because a
-- player asked for an answer (an invite) or it happens once (joining, leaving).
local cooldowns =
{
    death        = 300,
    tell         = 60,
    tell_hunting = 60,
    tell_party   = 60,
    tell_seeking = 60,
    tell_town    = 60,
}

-- Events whose lines are not said again anywhere on the server within this many seconds (idle chatter).
local noRepeat =
{
    idle_city = 3600,
}

local lastSaid = {}
local lineUsed = {}

-- The index of a line for the event: a random one, or for chatter one not said within the no-repeat time
-- (the one said longest ago when every line was said recently).
local function pickLine(event, choices)
    local window = noRepeat[event]
    if not window then
        return math.random(1, #choices)
    end

    lineUsed[event] = lineUsed[event] or {}
    local used      = lineUsed[event]
    local now       = GetSystemTime()
    local fresh     = {}
    local oldest    = 1
    for i = 1, #choices do
        if not used[i] or now - used[i] >= window then
            table.insert(fresh, i)
        end

        if (used[i] or 0) < (used[oldest] or 0) then
            oldest = i
        end
    end

    local index = #fresh > 0 and fresh[math.random(1, #fresh)] or oldest
    used[index] = now
    return index
end

-- Returns the line the bot says for the event, or nil to stay quiet.
xi.bots.chatLine = function(bot, event)
    local choices = lines[event]
    if not choices then
        return nil
    end

    local cooldown = cooldowns[event]
    if cooldown then
        -- Every tell reply shares one cooldown, whatever the bot is doing.
        local key = string.format('%d:%s', bot:getID(), string.find(event, '^tell') and 'tell' or event)
        local now = GetSystemTime()
        if lastSaid[key] and now - lastSaid[key] < cooldown then
            return nil
        end

        lastSaid[key] = now
    end

    local line = choices[pickLine(event, choices)]
    if type(line) == 'function' then
        line = line(bot)
    end

    return line
end

-- The number of lines an event has (tests check the chatter pool).
xi.bots.chatLineCount = function(event)
    return lines[event] and #lines[event] or 0
end

-- Forgets every chat cooldown (tests call it between cases).
xi.bots.resetChat = function()
    lastSaid = {}
    lineUsed = {}
end

-- The one line every character gets at login while bots are on, so players know who the AI adventurers are and
-- how to hide them. Sent to players who hid bots too: it reminds them how to show them again.
xi.bots.loginNotice = function(player)
    if not xi.settings.map.PLAYER_BOTS_ENABLED then
        return
    end

    player:printToPlayer('This server has AI adventurers. Their /check comment says "AI adventurer". ' ..
        'Type !bots off to hide them, or !bots on to show them again.')
end
