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

    refuse_busy =
    {
        'Sorry, I\'m already in a party.',
    },

    refuse_full =
    {
        'Looks like your party is full.',
    },

    refuse_trusts =
    {
        'Happy to join once your trusts are gone.',
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

    tell =
    {
        'Sorry, busy leveling right now.',
        'Hi! Can\'t talk much, I\'m fighting.',
        'Sorry, a bit busy at the moment.',
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
    },
}

-- Seconds before the same bot says a line for the event again. Events not listed always speak, because a
-- player asked for an answer (an invite) or it happens once (joining, leaving).
local cooldowns =
{
    death = 300,
    tell  = 60,
}

local lastSaid = {}

-- Returns the line the bot says for the event, or nil to stay quiet.
xi.bots.chatLine = function(bot, event)
    local choices = lines[event]
    if not choices then
        return nil
    end

    local cooldown = cooldowns[event]
    if cooldown then
        local key = string.format('%d:%s', bot:getID(), event)
        local now = GetSystemTime()
        if lastSaid[key] and now - lastSaid[key] < cooldown then
            return nil
        end

        lastSaid[key] = now
    end

    local line = choices[math.random(1, #choices)]
    if type(line) == 'function' then
        line = line(bot)
    end

    return line
end
