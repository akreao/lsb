-----------------------------------
-- Player bots: picks a bot's name, looks, job, level and gear (custom, not retail)
-----------------------------------
require('scripts/globals/bots/gear')
-----------------------------------
xi = xi or {}
xi.bots = xi.bots or {}

-- Plain generated names, the way players name characters: two or three syllables, letters only.
local nameStarts =
{
    'Al', 'Ar', 'Ba', 'Bel', 'Bri', 'Ca', 'Cor', 'Da', 'Del', 'Dra', 'E', 'El', 'Fa', 'Fen', 'Ga', 'Gil', 'Ha', 'Ia',
    'Ja', 'Jor', 'Ka', 'Kei', 'La', 'Lio', 'Ma', 'Mer', 'Na', 'Ny', 'O', 'Or', 'Pa', 'Ra', 'Ren', 'Ro', 'Sa', 'Sel',
    'Shi', 'Ta', 'Tor', 'Ul', 'Va', 'Vel', 'Wy', 'Xa', 'Ya', 'Yu', 'Za', 'Zel',
}

local nameMiddles = { 'ra', 'li', 'na', 'ri', 'do', 'la', 'ma', 'ro', 'ka', 'ne', 'shi', 'te', 'vi', 'zu' }

local nameEnds =
{
    'n', 'r', 'th', 'x', 'l', 's', 'k', 'ra', 'na', 'ia', 'el', 'on', 'an', 'is', 'us', 'or', 'en', 'ius', 'ette', 'wyn',
    'mir', 'dor', 'lan', 'ric', 'sha', 'tes', 'vin', 'ya',
}

local function pick(list)
    return list[math.random(1, #list)]
end

xi.bots.randomName = function()
    local name = pick(nameStarts)
    if math.random(1, 100) <= 40 then
        name = name .. pick(nameMiddles)
    end

    return name .. pick(nameEnds)
end

-- Race weights roughly follow how many players picked each race.
local raceWeights =
{
    { xi.race.HUME_M,   14 },
    { xi.race.HUME_F,   12 },
    { xi.race.ELVAAN_M, 12 },
    { xi.race.ELVAAN_F,  8 },
    { xi.race.TARU_M,   14 },
    { xi.race.TARU_F,   12 },
    { xi.race.MITHRA,   16 },
    { xi.race.GALKA,    12 },
}

local function weighted(entries)
    local total = 0
    for _, entry in ipairs(entries) do
        total = total + entry[2]
    end

    local roll = math.random(1, total)
    for _, entry in ipairs(entries) do
        roll = roll - entry[2]
        if roll <= 0 then
            return entry[1]
        end
    end

    return entries[1][1]
end

-- Jobs a player could have at this level: the advanced jobs open at 30, and each expansion's jobs need it on.
local baseJobs = { xi.job.WAR, xi.job.MNK, xi.job.WHM, xi.job.BLM, xi.job.RDM, xi.job.THF }

local function advancedJobs()
    local jobs =
    {
        xi.job.PLD, xi.job.DRK, xi.job.BST, xi.job.BRD, xi.job.RNG,
        xi.job.SAM, xi.job.NIN, xi.job.DRG, xi.job.SMN,
    }

    local main = xi.settings.main
    if main.ENABLE_TOAU == 1 then
        table.insert(jobs, xi.job.BLU)
        table.insert(jobs, xi.job.COR)
        table.insert(jobs, xi.job.PUP)
    end

    if main.ENABLE_WOTG == 1 then
        table.insert(jobs, xi.job.DNC)
        table.insert(jobs, xi.job.SCH)
    end

    if main.ENABLE_SOA == 1 then
        table.insert(jobs, xi.job.GEO)
        table.insert(jobs, xi.job.RUN)
    end

    return jobs
end

xi.bots.pickJob = function(level)
    if level < 30 then
        return pick(baseJobs)
    end

    -- Base jobs stay common at high level, as on retail.
    if math.random(1, 100) <= 35 then
        return pick(baseJobs)
    end

    return pick(advancedJobs())
end

-- Common support jobs per main job; the first one that is a base job or unlocked by level is used.
local supportJobs =
{
    [xi.job.WAR] = { xi.job.NIN, xi.job.MNK, xi.job.THF, xi.job.SAM },
    [xi.job.MNK] = { xi.job.WAR, xi.job.NIN, xi.job.THF },
    [xi.job.WHM] = { xi.job.BLM, xi.job.RDM, xi.job.SCH },
    [xi.job.BLM] = { xi.job.WHM, xi.job.RDM, xi.job.SCH },
    [xi.job.RDM] = { xi.job.WHM, xi.job.BLM, xi.job.NIN },
    [xi.job.THF] = { xi.job.WAR, xi.job.NIN, xi.job.DNC },
    [xi.job.PLD] = { xi.job.WAR, xi.job.NIN, xi.job.RDM, xi.job.BLU },
    [xi.job.DRK] = { xi.job.WAR, xi.job.SAM, xi.job.NIN },
    [xi.job.BST] = { xi.job.WHM, xi.job.WAR, xi.job.NIN },
    [xi.job.BRD] = { xi.job.WHM, xi.job.NIN, xi.job.RDM },
    [xi.job.RNG] = { xi.job.WAR, xi.job.NIN, xi.job.SAM },
    [xi.job.SAM] = { xi.job.WAR, xi.job.NIN, xi.job.DRG },
    [xi.job.NIN] = { xi.job.WAR, xi.job.THF, xi.job.DNC },
    [xi.job.DRG] = { xi.job.WAR, xi.job.SAM, xi.job.WHM },
    [xi.job.SMN] = { xi.job.WHM, xi.job.RDM, xi.job.SCH },
    [xi.job.BLU] = { xi.job.WAR, xi.job.NIN, xi.job.RDM },
    [xi.job.COR] = { xi.job.NIN, xi.job.WAR, xi.job.RNG },
    [xi.job.PUP] = { xi.job.WAR, xi.job.WHM, xi.job.DNC },
    [xi.job.DNC] = { xi.job.WAR, xi.job.NIN, xi.job.SAM },
    [xi.job.SCH] = { xi.job.RDM, xi.job.WHM, xi.job.BLM },
    [xi.job.GEO] = { xi.job.RDM, xi.job.WHM, xi.job.BLM },
    [xi.job.RUN] = { xi.job.WAR, xi.job.NIN, xi.job.BLU },
}

-- No support job before 18; from then on half the main level, as for a player who did the quest.
xi.bots.pickSupportJob = function(mainJob, level)
    if level < 18 then
        return xi.job.NONE, 0
    end

    local available = advancedJobs()
    for _, job in ipairs(supportJobs[mainJob] or {}) do
        local isBase = false
        for _, baseJob in ipairs(baseJobs) do
            isBase = isBase or baseJob == job
        end

        local isOpen = false
        for _, advanced in ipairs(available) do
            isOpen = isOpen or advanced == job
        end

        if job ~= mainJob and (isBase or isOpen) then
            return job, math.max(1, math.floor(level / 2))
        end
    end

    return xi.job.NONE, 0
end

local function maxBotLevel(zone)
    local maxLevel = math.min(xi.settings.map.PLAYER_BOTS_MAX_LEVEL or 75, xi.settings.main.MAX_LEVEL or 99)
    local restriction = zone and zone:levelRestriction() or 0
    if restriction and restriction > 0 then
        maxLevel = math.min(maxLevel, restriction)
    end

    return math.max(1, maxLevel)
end

-- The zone's level band, from the levels of its live normal mobs (not notorious monsters): the middle half.
xi.bots.zoneLevelBand = function(zone)
    local levels = {}
    for _, mob in ipairs(zone:getMobs()) do
        if
            mob:isSpawned() and
            mob:isAlive() and
            not mob:isNM() and
            mob:getMainLvl() > 0
        then
            table.insert(levels, mob:getMainLvl())
        end
    end

    if #levels == 0 then
        return nil
    end

    table.sort(levels)

    return levels[math.max(1, math.ceil(#levels * 0.25))], levels[math.max(1, math.ceil(#levels * 0.75))]
end

-- A level a player hunting here would have: a few levels above the mobs, so they check Easy Prey to Even Match.
-- Zones without mobs (cities) get any level, as players pass through towns at every level.
xi.bots.pickLevel = function(zone)
    local maxLevel = maxBotLevel(zone)
    local low, high = xi.bots.zoneLevelBand(zone)
    if not low then
        return math.random(1, maxLevel)
    end

    return utils.clamp(math.random(low, high) + math.random(1, 4), 1, maxLevel)
end

-- The fixed set for the highest gear band at or below the level.
xi.bots.gearFor = function(job, level)
    local sets = xi.bots.gear[job]
    if not sets then
        return {}
    end

    local chosen = {}
    for _, band in ipairs(xi.bots.gearBands) do
        if band <= level and sets[band] then
            chosen = sets[band]
        end
    end

    return chosen
end

-- Every gear entry must exist, fit its slot, and be wearable by the job at the band's first level.
-- Returns a list of problems (empty when the file is good).
xi.bots.checkGear = function()
    local problems = {}
    for job, sets in pairs(xi.bots.gear) do
        for band, set in pairs(sets) do
            for slot, itemId in pairs(set) do
                if not CanBotWear(itemId, job, band, slot) then
                    table.insert(problems, string.format('job %d band %d slot %d item %d', job, band, slot, itemId))
                end
            end
        end
    end

    return problems
end

local gearChecked = false

-- Builds a bot near pos and puts it in the zone. Fields of overrides (name, job, level, race, ...) win over the
-- random picks. Returns the bot, or nil when bots are off or the zone cannot take one.
xi.bots.spawn = function(zone, pos, overrides)
    overrides = overrides or {}

    if not gearChecked then
        gearChecked = true
        for _, problem in ipairs(xi.bots.checkGear()) do
            print(string.format('[bots] gear file: %s cannot be worn there', problem))
        end
    end

    local level    = overrides.level or xi.bots.pickLevel(zone)
    local job      = overrides.job or xi.bots.pickJob(level)
    local subJob, subLevel = xi.bots.pickSupportJob(job, level)
    local race     = overrides.race or weighted(raceWeights)

    local spec =
    {
        job      = job,
        level    = level,
        subJob   = overrides.subJob or subJob,
        subLevel = overrides.subLevel or subLevel,
        race     = race,
        face     = overrides.face or math.random(0, 15),
        size     = overrides.size or math.random(0, 2),
        x        = pos.x,
        y        = pos.y,
        z        = pos.z,
        rot      = pos.rot or math.random(0, 255),
        gear     = overrides.gear or xi.bots.gearFor(job, level),
    }

    -- A random name can clash with a character or a live bot; try a few.
    for _ = 1, 10 do
        spec.name = overrides.name or xi.bots.randomName()
        local bot = zone:spawnBot(spec)
        if bot or overrides.name then
            return bot
        end
    end

    return nil
end
