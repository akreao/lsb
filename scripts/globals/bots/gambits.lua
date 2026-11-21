-----------------------------------
-- Player bots: fighting behaviour per job, in the trust gambit language (custom, not retail)
-- Gambits run only while the bot is engaged; out of combat it rests instead. A gambit whose spell or
-- ability the bot does not have yet does nothing, so one list per job covers every level.
-- Weapon skills come from the bot's own weapon skills (used as soon as it has the TP).
-----------------------------------
require('scripts/globals/gambits')
-----------------------------------
xi = xi or {}
xi.bots = xi.bots or {}

local spellFamily = xi.magic.spellFamily

-- Healing and protection, for any bot whose jobs have the spells.
local function addSupport(bot)
    bot:addGambit(ai.t.SELF, { ai.c.HPP_LT, 40 }, { ai.r.MA, ai.s.HIGHEST, spellFamily.CURE })
    bot:addGambit(ai.t.PARTY, { ai.c.HPP_LT, 40 }, { ai.r.MA, ai.s.HIGHEST, spellFamily.CURE })
    bot:addGambit(ai.t.SELF, { ai.c.NOT_STATUS, xi.effect.PROTECT }, { ai.r.MA, ai.s.HIGHEST, spellFamily.PROTECT })
    bot:addGambit(ai.t.SELF, { ai.c.NOT_STATUS, xi.effect.SHELL }, { ai.r.MA, ai.s.HIGHEST, spellFamily.SHELL })
end

-- Shadows for anyone with Utsusemi (NIN, or a /NIN support job).
local function addShadows(bot)
    bot:addGambit(ai.t.SELF, { ai.c.NOT_STATUS, xi.effect.COPY_IMAGE }, { ai.r.MA, ai.s.HIGHEST, spellFamily.UTSUSEMI })
end

-- Debuffs and nukes for casters; nukes wait a minute between casts so MP lasts a fight.
local function addCaster(bot)
    bot:addGambit(ai.t.TARGET, { ai.c.NOT_STATUS, xi.effect.DIA }, { ai.r.MA, ai.s.HIGHEST, spellFamily.DIA }, 60)
    bot:addGambit(ai.t.TARGET, { ai.c.NOT_STATUS, xi.effect.PARALYSIS }, { ai.r.MA, ai.s.HIGHEST, spellFamily.PARALYZE }, 60)
    bot:addGambit(ai.t.TARGET, { ai.c.NOT_STATUS, xi.effect.SLOW }, { ai.r.MA, ai.s.HIGHEST, spellFamily.SLOW }, 60)
    bot:addGambit(ai.t.TARGET, { ai.c.MPP_GTE, 0 }, { ai.r.MA, ai.s.HIGHEST, spellFamily.NONE }, 30)
end

local byJob =
{
    [xi.job.WAR] = function(bot)
        bot:addGambit(ai.t.SELF, { ai.c.NOT_STATUS, xi.effect.BERSERK }, { ai.r.JA, ai.s.SPECIFIC, xi.ja.BERSERK })
    end,

    [xi.job.MNK] = function(bot)
        bot:addGambit(ai.t.SELF, { ai.c.HPP_LT, 50 }, { ai.r.JA, ai.s.SPECIFIC, xi.ja.CHAKRA })
        bot:addGambit(ai.t.SELF, { ai.c.NOT_STATUS, xi.effect.FOCUS }, { ai.r.JA, ai.s.SPECIFIC, xi.ja.FOCUS })
    end,

    [xi.job.WHM] = function(bot)
        bot:addGambit(ai.t.TARGET, { ai.c.NOT_STATUS, xi.effect.DIA }, { ai.r.MA, ai.s.HIGHEST, spellFamily.DIA }, 60)
        bot:addGambit(ai.t.TARGET, { ai.c.NOT_STATUS, xi.effect.PARALYSIS }, { ai.r.MA, ai.s.HIGHEST, spellFamily.PARALYZE }, 60)
    end,

    [xi.job.BLM] = addCaster,
    [xi.job.RDM] = addCaster,

    [xi.job.THF] = function(bot)
        bot:addGambit(ai.t.SELF, { ai.c.TP_GTE, 1000 }, { ai.r.JA, ai.s.SPECIFIC, xi.ja.SNEAK_ATTACK })
    end,

    [xi.job.PLD] = function(bot)
        bot:addGambit(ai.t.TARGET, { ai.c.NOT_STATUS, xi.effect.FLASH }, { ai.r.MA, ai.s.SPECIFIC, xi.magic.spell.FLASH }, 30)
    end,

    [xi.job.DRK] = function(bot)
        bot:addGambit(ai.t.SELF, { ai.c.NOT_STATUS, xi.effect.LAST_RESORT }, { ai.r.JA, ai.s.SPECIFIC, xi.ja.LAST_RESORT })
    end,

    [xi.job.SAM] = function(bot)
        bot:addGambit(ai.t.SELF, { ai.c.NOT_STATUS, xi.effect.HASSO }, { ai.r.JA, ai.s.SPECIFIC, xi.ja.HASSO })
        bot:addGambit(ai.t.SELF, { ai.c.TP_LT, 1000 }, { ai.r.JA, ai.s.SPECIFIC, xi.ja.MEDITATE })
    end,

    [xi.job.DRG] = function(bot)
        bot:addGambit(ai.t.TARGET, { ai.c.ALWAYS, 0 }, { ai.r.JA, ai.s.SPECIFIC, xi.ja.JUMP })
    end,

    [xi.job.SCH] = addCaster,
    [xi.job.GEO] = addCaster,
}

-- Gives a new bot the gambits for its jobs.
xi.bots.applyGambits = function(bot)
    addSupport(bot)
    addShadows(bot)

    for _, job in ipairs({ bot:getMainJob(), bot:getSubJob() }) do
        local apply = byJob[job]
        if apply then
            apply(bot)
        end
    end
end
