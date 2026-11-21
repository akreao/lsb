local ffi = require('ffi')
local packetReader = require('scripts/tests/helpers/packet_reader')

describe('Player bots', function()
    ---@type CClientEntityPair
    local player

    local zoneId = xi.zone.WEST_RONFAURE

    local function spawnBot(name)
        local zone = assert(player:getZone())
        return zone:spawnBot({
            name  = name,
            race  = xi.race.HUME_M,
            job   = xi.job.WAR,
            level = 10,
            x     = player:getXPos() + 2,
            y     = player:getYPos(),
            z     = player:getZPos(),
            gear  =
            {
                [xi.slot.MAIN] = xi.item.BRONZE_SWORD,
                [xi.slot.BODY] = xi.item.LEATHER_VEST,
            },
        })
    end

    local function countIncoming(packetType, uniqueNo)
        local count = 0
        for _, packet in ipairs(player.packets:getIncoming()) do
            if
                packet.type == packetType and
                packetReader.readU32(packet.data, 0x04) == uniqueNo
            then
                count = count + 1
            end
        end

        return count
    end

    before_each(function()
        xi.test.world:setSetting('map.PLAYER_BOTS_ENABLED', true)
        -- No automatic arrivals unless a test asks for them.
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_PER_ZONE', 0)
        player = xi.test.world:spawnPlayer({ zone = zoneId })
        player.packets:clear()
    end)

    -- Bots outlive a test's player by the leave delay; start each test with none.
    after_each(function()
        GetZone(zoneId):despawnBots()
    end)

    it('does nothing while the setting is off', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_ENABLED', false)
        assert(spawnBot('Offbot') == nil, 'bot spawned with the setting off')
        assert(GetZone(zoneId):getBotCount() == 0, 'zone has a bot with the setting off')
    end)

    it('is drawn as a player with a PC targid and a reserved id', function()
        local bot = assert(spawnBot('Testbot'), 'bot did not spawn')

        assert(bot:getID() >= 0x00F00000 and bot:getID() < 0x01000000, string.format('bad bot id 0x%X', bot:getID()))
        assert(bot:getTargID() >= 0x400 and bot:getTargID() < 0x700, string.format('bad bot targid 0x%X', bot:getTargID()))
        assert(bot:getTargID() ~= player:getTargID(), 'bot took the player targid')
        -- 0x00D: HP percent at 0x1E, in the first packet that carries the bot
        local hpp
        for _, packet in ipairs(player.packets:getIncoming()) do
            if
                not hpp and
                packet.type == 0x00D and
                packetReader.readU32(packet.data, 0x04) == bot:getID()
            then
                hpp = packet.data[0x1E]
            end
        end

        assert(hpp, 'player was not sent a 0x00D for the bot')
        assert(hpp == 100, string.format('0x00D sent the bot at %d%% HP', hpp))
        assert(bot:getMainLvl() == 10, 'bot level not set')
        assert(bot:getMaxHP() > 0, 'bot has no HP')
        assert(bot:getHP() == bot:getMaxHP() and bot:getHPP() == 100, string.format('bot spawned at %d/%d HP', bot:getHP(), bot:getMaxHP()))
        assert(bot:getMP() == bot:getMaxMP(), string.format('bot spawned at %d/%d MP', bot:getMP(), bot:getMaxMP()))
        assert(bot:getSubJob() == xi.job.NONE, 'bot without a support job has one')
        assert(bot:getSubLvl() == 0, 'bot without a support job has a support level')
    end)

    it('keeps a given support job', function()
        local zone = assert(player:getZone())
        local bot  = assert(zone:spawnBot({
            name     = 'Subbot',
            job      = xi.job.WAR,
            level    = 20,
            subJob   = xi.job.MNK,
            subLevel = 10,
            x        = player:getXPos(),
            y        = player:getYPos(),
            z        = player:getZPos(),
        }))

        assert(bot:getSubJob() == xi.job.MNK, 'support job not set')
        assert(bot:getSubLvl() == 10, 'support level not set')
    end)

    it('answers /check with its equipment', function()
        local bot = assert(spawnBot('Checkbot'))
        player.packets:clear()

        local packet = ffi.new('uint8_t[16]')
        packet[4]    = bit.band(bot:getID(), 0xFF)
        packet[5]    = bit.band(bit.rshift(bot:getID(), 8), 0xFF)
        packet[6]    = bit.band(bit.rshift(bot:getID(), 16), 0xFF)
        packet[7]    = bit.band(bit.rshift(bot:getID(), 24), 0xFF)
        packet[8]    = bit.band(bot:getTargID(), 0xFF)
        packet[9]    = bit.band(bit.rshift(bot:getTargID(), 8), 0xFF)
        packet[12]   = 0 -- Check
        player.packets:send(0x0DD, packet, ffi.sizeof(packet))

        local found = false
        for _, incoming in ipairs(player.packets:getIncoming()) do
            if incoming.type == 0x0C9 then
                found = true
            end
        end

        assert(found, '/check on the bot sent no 0x0C9')
    end)

    it('is a valid party invite target', function()
        local bot = assert(spawnBot('Partybot'))

        -- The first invite is held by the bot; a second one is refused because one is pending.
        player.actions:inviteToParty(bot)
        player.packets:clear()
        player.actions:inviteToParty(bot)

        local refused = false
        for _, incoming in ipairs(player.packets:getIncoming()) do
            if incoming.type == 0x009 then
                refused = true
            end
        end

        assert(refused, 'second invite was not refused, so the first never reached the bot')
    end)

    it('leaves a while after the last real player leaves the zone', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_LEAVE_DELAY', 30)
        assert(spawnBot('Leavebot'))
        assert(GetZone(zoneId):getBotCount() == 1, 'bot not counted')

        player:gotoZone(xi.zone.EAST_RONFAURE)
        for _ = 1, 10 do
            xi.test.world:skipTime(1)
        end

        assert(GetZone(zoneId):getBotCount() == 1, 'bot left before the leave delay')

        for _ = 1, 30 do
            xi.test.world:skipTime(1)
        end

        assert(GetZone(zoneId):getBotCount() == 0, 'bot stayed in an empty zone')
    end)

    it('arrive one at a time while a player is in the zone, up to the zone target', function()
        local zone = GetZone(zoneId)
        xi.test.world:setSetting('map.PLAYER_BOTS_ARRIVAL_INTERVAL', 5)
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_PER_ZONE', 3)
        assert(#zone:getBotEntrances() > 0, 'West Ronfaure has no arrival points')

        for _ = 1, 6 do
            xi.test.world:skipTime(1)
        end

        assert(zone:getBotCount() <= 2, 'bots arrived all at once')

        for _ = 1, 60 do
            xi.test.world:skipTime(1)
        end

        assert(zone:getBotCount() == 3, string.format('zone has %d bots, wanted the cap of 3', zone:getBotCount()))
    end)

    it('never populates instances, Dynamis or excluded zones', function()
        -- Stand-ins for zones this test map does not load.
        local function fakeZone(id, mask)
            local zone = {}

            function zone:getID()
                return id
            end

            function zone:getTypeMask()
                return mask
            end

            return zone
        end

        assert(xi.bots.isZoneAllowed(GetZone(zoneId)), 'West Ronfaure should allow bots')
        assert(not xi.bots.isZoneAllowed(fakeZone(xi.zone.FERETORY, xi.zoneType.CITY)), 'the Feretory allowed bots')
        assert(not xi.bots.isZoneAllowed(fakeZone(xi.zone.DYNAMIS_SAN_DORIA, xi.zoneType.DYNAMIS)), 'Dynamis allowed bots')
        assert(not xi.bots.isZoneAllowed(fakeZone(1, bit.bor(xi.zoneType.OUTDOORS, xi.zoneType.INSTANCED))), 'an instance allowed bots')
    end)

    it('writes nothing to the database', function()
        local writesBefore = GetBotDatabaseWrites()
        local bot = assert(spawnBot('Savebot'))

        -- The calls quest, mission and mob scripts make on a player.
        bot:setCharVar('[Bot]Test', 5)
        bot:incrementCharVar('[Bot]Test', 1)
        bot:addTitle(xi.title.NEW_ADVENTURER)
        bot:addKeyItem(xi.keyItem.MAP_OF_THE_SAN_DORIA_AREA)
        bot:addQuest(xi.questLog.SANDORIA, xi.quest.id.sandoria.A_SENTRYS_PERIL)
        bot:completeQuest(xi.questLog.SANDORIA, xi.quest.id.sandoria.A_SENTRYS_PERIL)
        bot:addFame(xi.fameArea.SANDORIA, 50)
        bot:addTeleport(xi.teleport.type.OUTPOST_SANDORIA, 1)
        bot:addSpell(xi.magic.spell.CURE)
        bot:addCurrency('spark_of_eminence', 100)
        bot:addGil(1000)
        bot:addItem(xi.item.PEBBLE)
        bot:addExp(5000)
        bot:addCapacityPoints(100)
        bot:setHomePoint()
        bot:addStatusEffect(xi.effect.PROTECT, { power = 10, duration = 60, origin = bot })

        -- A kill runs the mob's death scripts and the exp, sparks and records code with the bot as the killer.
        local mob = assert(player.entities:get(zones[zoneId].mob.FUNGUS_BEETLE))
        if not mob:isSpawned() then
            SpawnMob(mob:getID())
        end

        mob:updateClaim(bot)
        mob:takeDamage(2000000000, bot)
        xi.test.world:tickEntity(mob)
        for _ = 1, 5 do
            xi.test.world:skipTime(5)
            xi.test.world:tickEntity(mob)
        end

        assert(not mob:isAlive(), 'the bot did not kill the mob')

        -- Leaving the zone runs the char destructor (history, death time).
        GetZone(zoneId):despawnBots()

        local writes = GetBotDatabaseWrites() - writesBefore
        assert(writes == 0, string.format('bot wrote to the database %d time(s)', writes))
    end)

    it('has a gear file that every job can wear', function()
        local problems = xi.bots.checkGear()
        assert(#problems == 0, 'gear file problems: ' .. table.concat(problems, '; '))
    end)

    it('builds a bot that fits the zone, with its gear, skills and spells', function()
        local zone = assert(player:getZone())
        local low, high = xi.bots.zoneLevelBand(zone)
        assert(low and high, 'West Ronfaure has no mob level band')

        for _ = 1, 10 do
            local bot = assert(xi.bots.spawn(zone, { x = player:getXPos(), y = player:getYPos(), z = player:getZPos() }))
            local level = bot:getMainLvl()
            assert(level > low and level <= high + 4, string.format('level %d outside the zone band %d-%d', level, low, high))

            for slot, itemId in pairs(xi.bots.gearFor(bot:getMainJob(), level)) do
                assert(bot:getEquipID(slot) == itemId, string.format('%s is missing item %d in slot %d', bot:getName(), itemId, slot))
            end
        end

        local whm = assert(xi.bots.spawn(zone, { x = player:getXPos(), y = player:getYPos(), z = player:getZPos() },
            { name = 'Healbot', job = xi.job.WHM, level = 30 }))

        assert(whm:getSubJob() ~= xi.job.NONE and whm:getSubLvl() == 15, 'level 30 bot has no level 15 support job')
        assert(whm:hasSpell(xi.magic.spell.CURE_II), 'level 30 WHM bot cannot cast Cure II')
        assert(not whm:hasSpell(xi.magic.spell.CURE_IV), 'level 30 WHM bot knows Cure IV (level 48)')
        assert(whm:getSkillLevel(xi.skill.CLUB) > 0, 'WHM bot has no club skill')
        assert(whm:getSkillLevel(xi.skill.HEALING_MAGIC) > 0, 'WHM bot has no healing skill')

        -- A second weapon needs Dual Wield, which the bot must have before its gear goes on.
        local nin = assert(xi.bots.spawn(zone, { x = player:getXPos(), y = player:getYPos(), z = player:getZPos() },
            { name = 'Shadowbot', job = xi.job.NIN, level = 30 }))
        local ninGear = xi.bots.gearFor(xi.job.NIN, 30)
        assert(nin:getEquipID(xi.slot.SUB) == ninGear[xi.slot.SUB], 'NIN bot is missing its off-hand katana')
    end)

    it('never reuses a live name', function()
        assert(spawnBot('Twinbot'))
        assert(spawnBot('Twinbot') == nil, 'a second bot took a live name')
    end)

    -- A normal mob at least 40 yalms from the player, so no real player is near the bot's prey.
    local function findPrey()
        local zone = assert(player:getZone())
        local here = { x = player:getXPos(), y = player:getYPos(), z = player:getZPos() }
        for _, mob in ipairs(zone:getMobs()) do
            local there = { x = mob:getXPos(), y = mob:getYPos(), z = mob:getZPos() }
            if
                mob:isSpawned() and
                mob:isAlive() and
                not mob:isNM() and
                not mob:isEngaged() and
                utils.distance(here, there) > 40
            then
                return mob
            end
        end
    end

    local function spawnHunter(name, mob)
        return assert(xi.bots.spawn(assert(player:getZone()),
            { x = mob:getXPos() + 3, y = mob:getYPos(), z = mob:getZPos() },
            { name = name, job = xi.job.WAR, level = mob:getMainLvl() + 3 }))
    end

    it('hunts a mob that fits its level on its own, and writes nothing', function()
        local writesBefore = GetBotDatabaseWrites()
        local mob = assert(findPrey(), 'no mob far enough from the player')
        local bot = spawnHunter('Hunterbot', mob)

        local engaged = false
        for _ = 1, 30 do
            xi.test.world:skipTime(1)
            engaged = engaged or bot:isEngaged()
        end

        assert(engaged, 'bot never engaged a mob')

        for _ = 1, 150 do
            if not mob:isAlive() then
                break
            end

            xi.test.world:skipTime(1)
        end

        assert(not mob:isAlive() or mob:getHPP() < 100, 'bot never hurt its prey')
        assert(GetBotDatabaseWrites() == writesBefore, 'hunting wrote to the database')
    end)

    it('keeps to the hunter cap', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_HUNTERS', 1)
        local mob = assert(findPrey(), 'no mob far enough from the player')
        local first  = spawnHunter('Firstbot', mob)
        local second = spawnHunter('Secondbot', mob)

        local bothEngaged = false
        for _ = 1, 30 do
            xi.test.world:skipTime(1)
            bothEngaged = bothEngaged or (first:isEngaged() and second:isEngaged())
        end

        assert(not bothEngaged, 'two bots hunted with a cap of one')
    end)

    it('comes back at a zone entrance when nobody raises it', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_RAISE_WAIT', 5)
        local bot = assert(spawnBot('Diebot'))
        bot:setHP(0)
        xi.test.world:skipTime(2)
        assert(GetZone(zoneId):getBotCount() == 1, 'dead bot left before its raise wait')

        xi.test.world:skipTime(10)

        local bots = GetZone(zoneId):getBots()
        assert(#bots == 1, 'dead bot did not come back')
        assert(bots[1]:getName() == 'Diebot' and bots[1]:isAlive(), 'the bot that came back is not alive')
    end)

    it('can be cleared', function()
        local bot = assert(spawnBot('Clearbot'))
        local botId = bot:getID()
        player.packets:clear()

        GetZone(zoneId):despawnBots()

        assert(GetZone(zoneId):getBotCount() == 0, 'bot not cleared')
        assert(countIncoming(0x00D, botId) > 0, 'player was not told the bot despawned')
    end)
end)
