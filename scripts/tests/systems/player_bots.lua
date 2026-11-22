local ffi = require('ffi')
local packetReader = require('scripts/tests/helpers/packet_reader')

describe('Player bots', function()
    ---@type CClientEntityPair
    local player

    local zoneId = xi.zone.WEST_RONFAURE
    local noHuntZones

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

    local function zoneSpawnAt(name, x, y, z)
        return assert(player:getZone()):spawnBot({ name = name, job = xi.job.WAR, level = 10, x = x, y = y, z = z })
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
        -- West Ronfaure is a starting zone and the test player fits its level; let bots hunt unless a test says not.
        -- Table settings live only in Lua (setSetting takes plain values).
        noHuntZones = xi.settings.map.PLAYER_BOTS_NO_HUNT_ZONES
        xi.settings.map.PLAYER_BOTS_NO_HUNT_ZONES = {}
        xi.test.world:setSetting('map.PLAYER_BOTS_HUNTERS_YIELD', false)
        player = xi.test.world:spawnPlayer({ zone = zoneId })
        player.packets:clear()
    end)

    -- Bots outlive a test's player by the leave delay; start each test with none and no leftover bot state.
    after_each(function()
        ResetBots()
        xi.settings.map.PLAYER_BOTS_NO_HUNT_ZONES = noHuntZones
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

    it('arrive a few at once spread out of sight, then one at a time, up to the zone target', function()
        local zone = GetZone(zoneId)
        xi.test.world:setSetting('map.PLAYER_BOTS_ARRIVAL_INTERVAL', 5)
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_PER_ZONE', 5)
        assert(#zone:getBotEntrances() > 0, 'West Ronfaure has no arrival points')

        -- The zone's arrival timer may still run from an earlier test (default interval 20 s).
        for _ = 1, 25 do
            xi.test.world:skipTime(1)
            if zone:getBotCount() > 0 then
                break
            end
        end

        assert(zone:getBotCount() == 3, string.format('first wave brought %d bots, not 3', zone:getBotCount()))
        local here = { x = player:getXPos(), y = player:getYPos(), z = player:getZPos() }
        for _, bot in ipairs(zone:getBots()) do
            local there = { x = bot:getXPos(), y = bot:getYPos(), z = bot:getZPos() }
            assert(utils.distance(here, there) >= 60, 'a first-wave bot appeared in sight of the player')
        end

        local last = zone:getBotCount()
        for _ = 1, 30 do
            xi.test.world:skipTime(1)
            assert(zone:getBotCount() - last <= 1, 'after the first wave bots did not arrive one at a time')
            last = zone:getBotCount()
        end

        assert(zone:getBotCount() == 5, string.format('zone has %d bots, wanted the cap of 5', zone:getBotCount()))
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
        assert(CheckDatabaseForBots() == 0, 'a char table has a row with a bot id')
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

            -- Head and body may be swapped for another common piece (xi.bots.varyGear), but are never left empty.
            for slot, itemId in pairs(xi.bots.gearFor(bot:getMainJob(), level)) do
                if slot == xi.slot.HEAD or slot == xi.slot.BODY then
                    assert(bot:getEquipID(slot) ~= 0, string.format('%s has nothing in slot %d', bot:getName(), slot))
                else
                    assert(bot:getEquipID(slot) == itemId, string.format('%s is missing item %d in slot %d', bot:getName(), itemId, slot))
                end
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

    -- Party (phase 7) and chat (phase 8)

    -- Lets the bot's invite delay (2 to 6 seconds) run out.
    local function waitForAnswer()
        for _ = 1, 8 do
            xi.test.world:skipTime(1)
        end
    end

    -- Tells (0x017 kind 3) the player got from the named sender.
    local function tellsFrom(name)
        local count = 0
        for _, packet in ipairs(player.packets:getIncoming()) do
            if packet.type == 0x017 and packet.data[0x04] == 3 then
                local sender = {}
                for i = 0, 14 do
                    local byte = packet.data[0x08 + i]
                    if byte == 0 then
                        break
                    end

                    sender[#sender + 1] = string.char(byte)
                end

                if table.concat(sender) == name then
                    count = count + 1
                end
            end
        end

        return count
    end

    -- A bot answers one invite a minute from the same player; let that minute pass before inviting again.
    local function waitOutInviteLimit()
        for _ = 1, 60 do
            xi.test.world:skipTime(1)
        end
    end

    local function inviteBot(name)
        local bot = assert(spawnBot(name))
        player.actions:inviteToParty(bot)
        waitForAnswer()
        return bot
    end

    -- Packet 0x071 (kick) for the party member with this name.
    local function kick(name)
        local packet = ffi.new('uint8_t[28]')
        packet[0x0A] = 0 -- Party
        for i = 1, #name do
            packet[0x0B + i] = string.byte(name, i)
        end

        player.packets:send(0x071, packet, ffi.sizeof(packet))
    end

    it('joins a party a moment after an invite, and writes nothing', function()
        local writesBefore = GetBotDatabaseWrites()
        player:setLevel(10)
        local bot = assert(spawnBot('Joinbot'))

        player.actions:inviteToParty(bot)
        xi.test.world:skipTime(1)
        assert(player:getPartySize() == 1, 'bot joined at once, with no delay')

        player.packets:clear()
        waitForAnswer()

        assert(player:getPartySize() == 2, 'bot did not join the party')
        assert(bot:getPartySize() == 2, 'bot does not know it is in the party')
        assert(countIncoming(0x0DD, bot:getID()) > 0, 'party list sent to the leader has no bot')
        assert(GetBotDatabaseWrites() == writesBefore, 'joining a party wrote to the database')
    end)

    it('turns down a leader too far above or below it, and says why', function()
        player:setLevel(30)
        local bot = inviteBot('Lowbot')
        assert(bot:getPartySize() == 1, 'level 10 bot joined a level 30 leader')
        assert(tellsFrom('Lowbot') == 1, 'bot did not say why it turned the invite down')

        player:setLevel(1)
        waitOutInviteLimit()
        player.packets:clear()
        xi.test.world:setSetting('map.PLAYER_BOTS_PARTY_LEVEL_BELOW', 5)
        player.actions:inviteToParty(bot)
        waitForAnswer()
        assert(bot:getPartySize() == 1, 'level 10 bot joined a level 1 leader')
        assert(tellsFrom('Lowbot') == 1, 'bot did not say why it turned the invite down')
    end)

    it('goes back to hunting alone when dismissed', function()
        player:setLevel(10)
        local bot = inviteBot('Kickbot')
        assert(bot:getPartySize() == 2, 'bot did not join the party')

        player.packets:clear()
        kick('Kickbot')
        xi.test.world:skipTime(1)

        assert(bot:getPartySize() == 1, 'bot is still in the party after a kick')
        assert(player:getPartySize() == 1, 'leader still counts the bot')
        assert(tellsFrom('Kickbot') == 1, 'bot said nothing when dismissed')
        assert(GetZone(zoneId):getBotCount() == 1, 'dismissed bot left the zone')
    end)

    it('follows its leader through a zone line, at the level it reached', function()
        player:setLevel(10)
        local bot = inviteBot('Zonebot')
        assert(bot:getPartySize() == 2, 'bot did not join the party')
        bot:setLevel(12)
        local level = 12

        player:gotoZone(xi.zone.EAST_RONFAURE)
        for _ = 1, 3 do
            xi.test.world:skipTime(1)
        end

        assert(GetZone(zoneId):getBotCount() == 0, 'bot stayed behind when the leader zoned')
        local bots = GetZone(xi.zone.EAST_RONFAURE):getBots()
        assert(#bots == 1, 'bot did not arrive in the leader\'s new zone')
        assert(bots[1]:getName() == 'Zonebot', 'a different bot arrived')
        assert(bots[1]:getMainLvl() == level, string.format('bot came back at %d, wanted %d', bots[1]:getMainLvl(), level))
        assert(bots[1]:getPartySize() == 2, 'bot is not in the party after following')
        assert(player:getPartySize() == 2, string.format('leader counts %d members, wanted 2', player:getPartySize()))
        assert(utils.distance(bots[1]:getPos(), player:getPos()) < 10, 'bot did not arrive next to its leader')
    end)

    it('leaves the party when its leader does not come back', function()
        local excluded = xi.settings.map.PLAYER_BOTS_EXCLUDED_ZONES
        xi.settings.map.PLAYER_BOTS_EXCLUDED_ZONES = { xi.zone.EAST_RONFAURE }

        player:setLevel(10)
        local bot = inviteBot('Stayer')
        assert(bot:getPartySize() == 2, 'bot did not join the party')

        player:gotoZone(xi.zone.EAST_RONFAURE)
        xi.test.world:skipTime(5)
        local bots = GetZone(zoneId):getBots()
        assert(#bots == 1 and bots[1]:getPartySize() == 2, 'bot gave up on its leader at once')

        xi.test.world:skipTime(150)
        xi.settings.map.PLAYER_BOTS_EXCLUDED_ZONES = excluded

        bots = GetZone(zoneId):getBots()
        assert(#bots == 1, 'bot followed into an excluded zone')
        assert(bots[1]:getPartySize() == 1, 'bot is still in the party after waiting')
    end)

    it('shows in the party window', function()
        player:setLevel(10)
        local bot = inviteBot('Listbot')
        assert(bot:getPartySize() == 2, 'bot did not join')

        player.packets:clear()
        player.actions:inviteToParty(assert(spawnBot('Listbot2')))
        waitForAnswer()

        local listed = false
        for _, packet in ipairs(player.packets:getIncoming()) do
            if packet.type == 0x0C8 then
                for i = 0, 19 do
                    if packetReader.readU32(packet.data, 0x08 + 12 * i) == bot:getID() then
                        listed = true
                    end
                end
            end
        end

        assert(listed, 'party table (0x0C8) has no entry for the bot')
    end)

    it('heals a party member as a healer', function()
        player:setLevel(20)
        local bot = assert(player:getZone()):spawnBot({ name = 'Healbot', job = xi.job.WHM, level = 20, x = player:getXPos() + 2, y = player:getYPos(), z = player:getZPos() })
        player.actions:inviteToParty(bot)
        waitForAnswer()
        assert(bot:getPartySize() == 2, 'healer did not join')

        player:setHP(math.floor(player:getMaxHP() / 4))
        local before = player:getHP()
        for _ = 1, 15 do
            xi.test.world:skipTime(1)
        end

        assert(player:getHP() > before, 'healer bot did not cure a hurt party member')
    end)

    it('passes every item in the party treasure pool', function()
        player:setLevel(10)
        local bot = inviteBot('Lootbot')
        assert(bot:getPartySize() == 2, 'bot did not join the party')

        player:addTreasure(xi.item.PEBBLE)

        local passed = false
        for _, item in ipairs(player:getTreasurePool():getItems()) do
            if item.id == xi.item.PEBBLE then
                for _, lotter in ipairs(item.lotters) do
                    if lotter.member:getID() == bot:getID() and lotter.lot == 0 then
                        passed = true
                    end
                end
            end
        end

        assert(passed, 'bot did not pass the item')
    end)

    it('counts in the party exp split as a trust does', function()
        player:setLevel(5)
        local bot = inviteBot('Expbot')
        assert(bot:getPartySize() == 2, 'bot did not join the party')

        local calls = {}
        local calculate = xi.experiencePoints.calculate
        xi.experiencePoints.calculate = function(member, mob, data)
            calls[member:getID()] = { partySize = data.partySize, highest = data.highestMemberLevel }
            return calculate(member, mob, data)
        end

        local mob = player.entities:moveTo('Fungus_Beetle')
        mob:setLevelRange(10, 10)
        mob:respawn()
        mob:updateClaim(player)
        bot:setPos(player:getXPos(), player:getYPos(), player:getZPos())
        mob:takeDamage(2000000000, player)
        for _ = 1, 5 do
            xi.test.world:tickEntity(mob)
            xi.test.world:skipTime(1)
        end

        xi.experiencePoints.calculate = calculate

        local mine = calls[player:getID()]
        assert(mine, 'player got no exp')
        assert(mine.partySize == 2, string.format('bot not counted in the party size (%d)', mine.partySize))
        assert(mine.highest == 10, string.format('bot not counted as the top level (%d)', mine.highest))
    end)

    it('takes at most two bots in a party', function()
        player:setLevel(10)
        local bots = { inviteBot('Capa'), inviteBot('Capb') }
        assert(player:getPartySize() == 3, 'first two bots did not join')

        player.packets:clear()
        local third = inviteBot('Capc')
        assert(third:getPartySize() == 1, 'a third bot joined')
        assert(tellsFrom('Capc') == 1, 'third bot did not say why')
        assert(#bots == 2)
    end)

    it('checks the level gap against the party\'s highest real member', function()
        player:setLevel(10)
        local friend = xi.test.world:spawnPlayer({ zone = zoneId, level = 40 })
        friend:setPos(player:getXPos() + 1, player:getYPos(), player:getZPos())
        player.actions:inviteToParty(friend)
        friend.actions:acceptPartyInvite()

        local bot = inviteBot('Gapbot')
        assert(bot:getPartySize() == 1, 'level 10 bot joined a party with a level 40 member')
    end)

    it('is level synced with its party', function()
        player:setLevel(20)
        local target = xi.test.world:spawnPlayer({ zone = zoneId, level = 12 })
        target:setPos(player:getXPos() + 1, player:getYPos(), player:getZPos())
        player.actions:inviteToParty(target)
        target.actions:acceptPartyInvite()

        local zone = assert(player:getZone())
        local early = assert(zone:spawnBot({ name = 'Synca', job = xi.job.WAR, level = 18, x = player:getXPos() + 2, y = player:getYPos(), z = player:getZPos() }))
        player.actions:inviteToParty(early)
        waitForAnswer()
        assert(early:getPartySize() == 3, 'first bot did not join')

        player.actions:setLevelSync(target)
        xi.test.world:skipTime(1)
        assert(early:getMainLvl() == 12, string.format('bot in the party was not synced (level %d)', early:getMainLvl()))

        local late = assert(zone:spawnBot({ name = 'Syncb', job = xi.job.WAR, level = 18, x = player:getXPos() + 2, y = player:getYPos(), z = player:getZPos() }))
        player.actions:inviteToParty(late)
        waitForAnswer()
        assert(late:getPartySize() == 4, 'second bot did not join')
        assert(late:getMainLvl() == 12, string.format('bot joining a synced party was not synced (level %d)', late:getMainLvl()))
    end)

    it('will not lead or be the sync target', function()
        player:setLevel(20)
        local bot = assert(player:getZone()):spawnBot({ name = 'Leadbot', job = xi.job.WAR, level = 20, x = player:getXPos() + 2, y = player:getYPos(), z = player:getZPos() })
        player.actions:inviteToParty(bot)
        waitForAnswer()
        assert(bot:getPartySize() == 2, 'bot did not join')

        player.packets:clear()
        player.actions:setPartyLeader(bot)
        xi.test.world:skipTime(1)
        local leader = player:getPartyLeader()
        assert(leader and leader:getID() == player:getID(), 'leadership went to the bot')
        assert(countIncoming(0x009, player:getID()) > 0, 'player was not told the hand-off failed')

        player.actions:setLevelSync(bot)
        xi.test.world:skipTime(1)
        assert(not player:hasStatusEffect(xi.effect.LEVEL_SYNC), 'party synced to a bot')
    end)

    it('joins a party that has trusts', function()
        player:setLevel(10)
        player:spawnTrust(xi.magic.spell.SHANTOTTO)
        xi.test.world:skipTime(1)
        assert(#player:getPartyWithTrusts() == 2, 'trust was not summoned')

        local bot = inviteBot('Trustbot')
        assert(bot:getPartySize() == 2, 'bot did not join a party with a trust')
        assert(#player:getPartyWithTrusts() == 3, 'the trust left when the bot joined')
    end)

    it('turns down an invite while a real player of that level is seeking a party', function()
        player:setLevel(10)
        local seeker = xi.test.world:spawnPlayer({ zone = zoneId, level = 11 })
        seeker.actions:setSeekingParty(true)
        xi.test.world:skipTime(1)

        local bot = inviteBot('Seekbot')
        assert(bot:getPartySize() == 1, 'bot joined while a real player was seeking')
        assert(tellsFrom('Seekbot') == 1, 'bot did not point at the seeker')

        seeker:setLevel(40)
        waitOutInviteLimit()
        player.actions:inviteToParty(bot)
        waitForAnswer()
        assert(bot:getPartySize() == 2, 'a seeker of another level kept the bot out')
    end)

    it('runs to a far leader instead of appearing next to it', function()
        player:setLevel(10)
        local bot = inviteBot('Runbot')
        assert(bot:getPartySize() == 2, 'bot did not join')

        local start = bot:getPos()
        player.entities:moveTo('Fungus_Beetle')
        local far = utils.distance(start, player:getPos())
        assert(far > 50, string.format('test needs a leader over 50 yalms away (%.1f)', far))

        xi.test.world:skipTime(1)
        local gap = utils.distance(bot:getPos(), player:getPos())
        assert(gap > 30, string.format('bot covered %.1f yalms in a second', far - gap))
    end)

    it('answers a tell a moment later', function()
        assert(spawnBot('Tellbot'))

        local message = 'hello there'
        local size    = 0x15 + #message + 1
        size          = size + (4 - size % 4) % 4
        local packet  = ffi.new('uint8_t[?]', size)
        packet[0x04]  = 3
        local name    = 'Tellbot'
        for i = 1, #name do
            packet[0x05 + i] = string.byte(name, i)
        end

        for i = 1, #message do
            packet[0x14 + i] = string.byte(message, i)
        end

        player.packets:clear()
        player.packets:send(0x0B6, packet, size)
        assert(tellsFrom('Tellbot') == 0, 'bot answered at once')

        waitForAnswer()
        assert(tellsFrom('Tellbot') == 1, 'bot did not answer the tell')
    end)

    it('gives up its client draw slot to a real player', function()
        -- Fill all 32 slots the client draws other characters with bots, then bring a real player close.
        local letters = 'abcdefghijklmnopqrstuvwxyzABCDEF'
        for i = 1, 32 do
            assert(spawnBot('Crowd' .. letters:sub(i, i)), 'crowd bot did not spawn')
        end

        -- Position updates (0x015) ask the zone to work out which characters this client draws.
        local function moveAndWait()
            for _ = 1, 5 do
                player.actions:move(player:getXPos(), player:getYPos(), player:getZPos())
                xi.test.world:skipTime(1)
            end
        end

        moveAndWait()

        local other = xi.test.world:spawnPlayer({ zone = zoneId })
        other:setPos(player:getXPos() + 5, player:getYPos(), player:getZPos())
        player.packets:clear()
        moveAndWait()

        assert(countIncoming(0x00D, other:getID()) > 0, 'the real player was not drawn while bots held every slot')
    end)

    it('fights back when a mob attacks it, where players can see the swings', function()
        local mob = assert(player.entities:moveTo('Fungus_Beetle'))
        mob:setLevelRange(10, 10)
        mob:respawn()
        local bot = assert(zoneSpawnAt('Fightbot', mob:getXPos() + 1, mob:getYPos(), mob:getZPos()))
        bot:setLevel(10)
        player.packets:clear()

        mob:updateEnmity(bot)
        for _ = 1, 10 do
            xi.test.world:skipTime(1)
        end

        assert(bot:isEngaged(), 'bot did not fight back')
        local actions = 0
        for _, packet in ipairs(player.packets:getIncoming()) do
            if
                packet.type == 0x028 and
                packetReader.readU32(packet.data, 0x05) == bot:getID()
            then
                actions = actions + 1
            end
        end

        assert(actions > 0, 'player saw none of the bot\'s actions')
    end)

    it('runs to its leader as fast as a player runs', function()
        player:setLevel(10)
        local bot = inviteBot('Sprinter')
        assert(bot:getPartySize() == 2, 'bot did not join')

        player.entities:moveTo('Fungus_Beetle')
        local before = utils.distance(bot:getPos(), player:getPos())
        for _ = 1, 10 do
            xi.test.world:skipTime(1)
        end

        -- The test world runs one 400 ms zone tick per step. A PC at the base speed of 50 runs 5 yalms a second
        -- (2 a tick); the old mob rate gave about 1.2 a tick.
        local covered = before - utils.distance(bot:getPos(), player:getPos())
        assert(covered > 16, string.format('bot closed %.1f yalms in 10 ticks', covered))
    end)

    it('wanders off on its own instead of hanging around a player', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_HUNTERS', 0)
        local bot = assert(spawnBot('Wanderer'))

        for _ = 1, 120 do
            xi.test.world:skipTime(1)
        end

        local gap = utils.distance(player:getPos(), bot:getPos())
        assert(gap > 20, string.format('bot is still %.1f yalms from the player after two minutes', gap))
    end)

    it('keeps its feet on the ground over hills', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_HUNTERS', 0)
        local zone = assert(player:getZone())
        local bot = assert(spawnBot('Hillbot'))

        local worst, samples = 0, 0
        for _ = 1, 120 do
            xi.test.world:skipTime(1)
            local y = bot:getYPos()
            -- The floor straight below a point above the bot: a buried bot is under it, a floating one above it
            -- (from 3 yalms up, so a tree or ledge overhead is not taken for the floor).
            local ground = zone:getGroundHeight(bot:getXPos(), y - 3, bot:getZPos(), y + 10)
            if ground then
                worst = math.max(worst, math.abs(ground - y))
                samples = samples + 1
            end
        end

        assert(samples > 100, string.format('only %d of 120 points had a floor', samples))
        assert(worst < 0.5, string.format('bot was up to %.2f yalms off the floor', worst))
    end)

    it('never climbs a ledge, as a player cannot', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_HUNTERS', 0)
        local zone = assert(player:getZone())
        local bots = {}
        for i = 1, 4 do
            bots[i] = assert(zone:spawnBot({
                name  = 'Climber' .. string.char(96 + i),
                job   = xi.job.WAR,
                level = 10,
                x     = player:getXPos() + i,
                y     = player:getYPos(),
                z     = player:getZPos(),
                gear  = { [xi.slot.MAIN] = xi.item.BRONZE_SWORD },
            }))
        end

        -- West Ronfaure has ledges that the navmesh links for chasing mobs. Measure on the navmesh floor, which slopes
        -- 46 degrees at most where it can be walked. Down is +y, so a rise is a drop in y.
        local climbs, last = {}, {}
        for _ = 1, 450 do
            xi.test.world:skipTime(1)
            for i, bot in ipairs(bots) do
                local x, z = bot:getXPos(), bot:getZPos()
                local here = { x = x, y = zone:getNavMeshHeight(x, bot:getYPos(), z), z = z }
                if last[i] and last[i].y and here.y then
                    local flat = math.sqrt((here.x - last[i].x) ^ 2 + (here.z - last[i].z) ^ 2)
                    local rise = last[i].y - here.y
                    if rise > 1.0 and rise > flat * 1.2 then
                        table.insert(climbs, string.format('%s rose %.1f over %.1f at (%.1f, %.1f, %.1f)', bot:getName(), rise, flat, here.x, here.y, here.z))
                    end
                end

                last[i] = here
            end
        end

        assert(#climbs == 0, 'bots climbed ledges: ' .. table.concat(climbs, '; ', 1, math.min(#climbs, 5)))
    end)

    it('stands on the ground, not above it', function()
        local bot = assert(zoneSpawnAt('Groundbot', player:getXPos() + 1, player:getYPos() - 1.5, player:getZPos()))
        xi.test.world:skipTime(1)

        local gap = math.abs(bot:getYPos() - player:getYPos())
        assert(gap < 0.5, string.format('bot stands %.2f yalms off the floor the player stands on', gap))
    end)

    it('turns down a trade at once', function()
        local bot    = assert(spawnBot('Tradebot'))
        local packet = ffi.new('uint8_t[12]')
        for i = 0, 3 do
            packet[0x04 + i] = bit.band(bit.rshift(bot:getID(), 8 * i), 0xFF)
        end

        packet[0x08] = bit.band(bot:getTargID(), 0xFF)
        packet[0x09] = bit.rshift(bot:getTargID(), 8)
        player.packets:clear()
        player.packets:send(0x032, packet, ffi.sizeof(packet))

        local cancelled = false
        for _, incoming in ipairs(player.packets:getIncoming()) do
            if
                incoming.type == 0x022 and
                packetReader.readU32(incoming.data, 0x08) == 1
            then
                cancelled = true
            end
        end

        assert(cancelled, 'trade with a bot was not turned down')
    end)

    it('leaves when bots are turned off', function()
        assert(spawnBot('Offlater'))
        xi.test.world:setSetting('map.PLAYER_BOTS_ENABLED', false)
        xi.test.world:skipTime(1)
        assert(GetZone(zoneId):getBotCount() == 0, 'bot stayed after bots were turned off')
    end)

    it('comes back at the level it reached', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_RAISE_WAIT', 5)
        local bot = assert(spawnBot('Growbot'))
        bot:setLevel(12)
        bot:setHP(0)
        for _ = 1, 10 do
            xi.test.world:skipTime(1)
        end

        local bots = GetZone(zoneId):getBots()
        assert(#bots == 1 and bots[1]:isAlive(), 'dead bot did not come back')
        assert(bots[1]:getMainLvl() == 12, string.format('bot came back at level %d, not 12', bots[1]:getMainLvl()))
    end)

    it('ends the party when the leader leaves a party of bots', function()
        player:setLevel(10)
        local bots = { inviteBot('Trioa'), inviteBot('Triob') }
        assert(player:getPartySize() == 3, 'bots did not join the party')

        player.actions:leaveParty()
        xi.test.world:skipTime(1)

        for _, bot in ipairs(bots) do
            assert(bot:getPartySize() == 1, bot:getName() .. ' is still in a party')
        end
    end)

    it('stays with the new leader when the old one leaves a party of two players and two bots', function()
        player:setLevel(10)
        local other = xi.test.world:spawnPlayer({ zone = zoneId, level = 10 })
        other:setPos(player:getXPos() + 1, player:getYPos(), player:getZPos())
        player.actions:inviteToParty(other)
        other.actions:acceptPartyInvite()
        local bots = { inviteBot('Duoa'), inviteBot('Duob') }
        assert(player:getPartySize() == 4, string.format('party has %d members, wanted 4', player:getPartySize()))

        player.actions:leaveParty()
        xi.test.world:skipTime(1)
        xi.test.world:skipTime(1)

        assert(other:getPartySize() == 3, string.format('new leader has %d members, wanted 3', other:getPartySize()))
        for _, bot in ipairs(bots) do
            assert(bot:getPartySize() == 3, bot:getName() .. ' left the party with the old leader')
        end
    end)

    it('never hunts in a starting zone', function()
        xi.settings.map.PLAYER_BOTS_NO_HUNT_ZONES = { zoneId }
        local mob = assert(findPrey(), 'no mob far enough from the player')
        local bot = spawnHunter('Newbiebot', mob)

        for _ = 1, 20 do
            xi.test.world:skipTime(1)
            assert(not bot:isEngaged(), 'bot hunted in a starting zone')
        end
    end)

    it('stays out of a real player\'s camp', function()
        -- The player fights a beetle, then walks off: the camp outlasts the fight.
        local target = assert(player.entities:moveTo('Fungus_Beetle'))
        target:respawn()
        player.actions:engage(target)
        xi.test.world:skipTime(1)
        assert(player:isEngaged(), 'player did not engage')

        local camp = { x = player:getXPos(), y = player:getYPos(), z = player:getZPos() }
        player:disengage()

        -- Another mob inside the camp, 25 to 50 yalms from where the player fought.
        local mob
        for _, other in ipairs(assert(player:getZone()):getMobs()) do
            local range = utils.distance(camp, { x = other:getXPos(), y = other:getYPos(), z = other:getZPos() })
            if
                not mob and
                other:isSpawned() and
                other:isAlive() and
                not other:isNM() and
                not other:isEngaged() and
                range > 25 and
                range < 50
            then
                mob = other
            end
        end

        assert(mob, 'no mob inside the camp')
        player:setPos(camp.x + 70, camp.y, camp.z)

        local bot = spawnHunter('Campbot', mob)
        for _ = 1, 20 do
            xi.test.world:skipTime(1)
            assert(not bot:isEngaged() or bot:getTarget():getID() ~= mob:getID(), 'bot hunted inside a player camp')
        end
    end)

    it('knows an NM placeholder from its script', function()
        assert(IsBotPlaceholderScript('Konschtat_Highlands', 'Tremor_Ram'), 'Tremor Ram is a placeholder')
        assert(not IsBotPlaceholderScript('West_Ronfaure', 'Fungus_Beetle'), 'Fungus Beetle is no placeholder')
    end)

    it('gives up its claim when it dies', function()
        local mob = assert(findPrey(), 'no mob far enough from the player')
        local bot = spawnHunter('Claimbot', mob)
        mob:updateClaim(bot)
        assert(bot:hasClaim(mob), 'bot did not claim the mob')

        bot:setHP(0)
        xi.test.world:skipTime(1)

        assert(not bot:hasClaim(mob), 'dead bot kept its claim')
    end)

    it('stops hunting when three players of the zone level are there', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_HUNTERS_YIELD', true)
        for _ = 1, 2 do
            xi.test.world:spawnPlayer({ zone = zoneId, level = player:getMainLvl() })
        end

        local mob = assert(findPrey(), 'no mob far enough from the player')
        local bot = spawnHunter('Yieldbot', mob)
        for _ = 1, 20 do
            xi.test.world:skipTime(1)
            -- It may still fight back against a mob that went for it first.
            local target = bot:isEngaged() and bot:getTarget()
            local attacked = target and target:getTarget() and target:getTarget():getID() == bot:getID()
            assert(not bot:isEngaged() or attacked, 'bot hunted with three fitting players in the zone')
        end
    end)

    -- Batch 4: chat and honesty

    -- Text of every tell the player got from the named sender.
    local function tellTexts(name)
        local texts = {}
        for _, packet in ipairs(player.packets:getIncoming()) do
            if packet.type == 0x017 and packet.data[0x04] == 3 then
                local sender = {}
                for i = 0, 14 do
                    local byte = packet.data[0x08 + i]
                    if byte == 0 then
                        break
                    end

                    sender[#sender + 1] = string.char(byte)
                end

                if table.concat(sender) == name then
                    local text = {}
                    for i = 0x17, packet.size - 1 do
                        local byte = packet.data[i]
                        if byte == 0 then
                            break
                        end

                        text[#text + 1] = string.char(byte)
                    end

                    table.insert(texts, table.concat(text))
                end
            end
        end

        return texts
    end

    local function sendTell(name, message)
        local size   = 0x15 + #message + 1
        size         = size + (4 - size % 4) % 4
        local packet = ffi.new('uint8_t[?]', size)
        packet[0x04] = 3
        for i = 1, #name do
            packet[0x05 + i] = string.byte(name, i)
        end

        for i = 1, #message do
            packet[0x14 + i] = string.byte(message, i)
        end

        player.packets:send(0x0B6, packet, size)
    end

    -- Packet 0x03D: add (mode 0) or remove (mode 1) a name on the player's blacklist.
    local function blacklist(name, mode)
        local packet = ffi.new('uint8_t[28]')
        for i = 1, #name do
            packet[0x07 + i] = string.byte(name, i)
        end

        packet[0x18] = mode
        player.packets:send(0x03D, packet, ffi.sizeof(packet))
    end

    it('asks for an invite in a tell while it seeks a party', function()
        assert(player:getZone()):spawnBot({ name = 'Seekbot', job = xi.job.WAR, level = 10, seeking = true,
            x = player:getXPos() + 2, y = player:getYPos(), z = player:getZPos() })

        player.packets:clear()
        sendTell('Seekbot', 'hi')
        waitForAnswer()

        local texts = tellTexts('Seekbot')
        assert(#texts == 1, string.format('expected one tell, got %d', #texts))
        assert(string.find(texts[1], 'invite'), 'seeking bot did not ask for an invite: ' .. texts[1])
    end)

    it('has 30 or more idle lines and repeats none within an hour', function()
        local bot   = assert(spawnBot('Chatbot'))
        local count = xi.bots.chatLineCount('idle_city')
        assert(count >= 30, string.format('only %d idle lines', count))

        local seen = {}
        -- The first line names the bot's job and level; the rest are fixed text.
        for _ = 1, count do
            local line = assert(xi.bots.chatLine(bot, 'idle_city'))
            assert(not seen[line], 'idle line said twice within an hour: ' .. line)
            seen[line] = true
        end
    end)

    it('answers only one invite a minute from the same player', function()
        player:setLevel(40)
        local bot = assert(spawnBot('Spambot'))

        player.packets:clear()
        player.actions:inviteToParty(bot)
        waitForAnswer()
        player.actions:inviteToParty(bot)
        waitForAnswer()
        assert(#tellTexts('Spambot') == 1, 'bot answered a second invite within a minute')

        for _ = 1, 60 do
            xi.test.world:skipTime(1)
        end

        player.actions:inviteToParty(bot)
        waitForAnswer()
        assert(#tellTexts('Spambot') == 2, 'bot ignored an invite after a minute')
    end)

    it('goes on the blacklist for the session and stops talking to that player', function()
        assert(spawnBot('Rudebot'))
        player.packets:clear()
        blacklist('Rudebot', 0)

        local added = false
        for _, packet in ipairs(player.packets:getIncoming()) do
            if packet.type == 0x042 and packet.data[0x18] == 0 then
                added = true
            end
        end

        assert(added, 'blacklisting a bot was not confirmed')

        player.packets:clear()
        sendTell('Rudebot', 'hello')
        waitForAnswer()
        assert(#tellTexts('Rudebot') == 0, 'a blacklisted bot still sent a tell')

        -- The full list sent at zone-in carries the bot.
        player.packets:clear()
        player.packets:send(0x03C, ffi.new('uint8_t[28]'), 28)
        local listed = false
        for _, packet in ipairs(player.packets:getIncoming()) do
            if packet.type == 0x041 and packet.data[0xF5] > 0 then
                listed = true
            end
        end

        assert(listed, 'the blacklist sent to the client left the bot out')

        blacklist('Rudebot', 1)
        player.packets:clear()
        for _ = 1, 60 do
            xi.test.world:skipTime(1)
        end

        sendTell('Rudebot', 'hello again')
        waitForAnswer()
        assert(#tellTexts('Rudebot') == 1, 'bot stayed quiet after it came off the blacklist')
    end)

    it('shows the bot note in the /check window', function()
        local bot = assert(spawnBot('Notebot'))
        player.packets:clear()

        local packet = ffi.new('uint8_t[16]')
        packet[4]    = bit.band(bot:getID(), 0xFF)
        packet[5]    = bit.band(bit.rshift(bot:getID(), 8), 0xFF)
        packet[6]    = bit.band(bit.rshift(bot:getID(), 16), 0xFF)
        packet[7]    = bit.band(bit.rshift(bot:getID(), 24), 0xFF)
        packet[8]    = bit.band(bot:getTargID(), 0xFF)
        packet[9]    = bit.band(bit.rshift(bot:getTargID(), 8), 0xFF)
        player.packets:send(0x0DD, packet, ffi.sizeof(packet))

        local note = xi.settings.map.PLAYER_BOTS_CHECK_NOTE
        local text
        for _, incoming in ipairs(player.packets:getIncoming()) do
            if incoming.type == 0x0CA then
                local chars = {}
                for i = 0x04, 0x04 + #note - 1 do
                    chars[#chars + 1] = string.char(incoming.data[i])
                end

                text = table.concat(chars)
            end
        end

        assert(text == note, string.format('/check note is %q, not %q', tostring(text), note))
    end)

    it('now and then cures a nearly dead stranger close by', function()
        -- Paused bots start no hunts, so the healer stays free to help.
        SetBotsPaused(true)
        player:setLevel(20)
        local bot = assert(player:getZone()):spawnBot({ name = 'Kindbot', job = xi.job.WHM, level = 20,
            x = player:getXPos() + 3, y = player:getYPos(), z = player:getZPos() })

        player:setHP(math.floor(player:getMaxHP() / 5))
        local before = player:getHP()
        for _ = 1, 90 do
            xi.test.world:skipTime(1)
            if player:getHP() > before then
                break
            end
        end

        assert(player:getHP() > before, 'a healer bot never cured a nearly dead stranger')
        assert(bot:getPartySize() == 1, 'the bot joined a party to cure')
        SetBotsPaused(false)
    end)

    -- Batch 5: presence and load

    it('is hidden from a player who turned bots off, and never hunts near them', function()
        player:setCharVar('BotsOptOut', 1)
        local bot = assert(spawnBot('Hiddenbot'))
        player.packets:clear()
        for _ = 1, 5 do
            player.actions:move(player:getXPos(), player:getYPos(), player:getZPos())
            xi.test.world:skipTime(1)
        end

        assert(countIncoming(0x00D, bot:getID()) == 0, 'a bot was drawn for a player who turned bots off')

        -- A mob 30 yalms from the opted-out player is off limits, though no player is within 25 of it.
        local mob = assert(findPrey(), 'no mob far enough from the player')
        player:setPos(mob:getXPos() + 30, mob:getYPos(), mob:getZPos())
        local hunter = spawnHunter('Shybot', mob)
        for _ = 1, 20 do
            xi.test.world:skipTime(1)
            assert(not (hunter:isEngaged() and hunter:getTarget() and hunter:getTarget():getID() == mob:getID()),
                'bot hunted a mob near a player who turned bots off')
        end

        player:setCharVar('BotsOptOut', 0)
    end)

    it('keeps to the server-wide cap', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_TOTAL', 2)
        assert(spawnBot('Capbota'))
        assert(spawnBot('Capbotb'))
        assert(spawnBot('Capbotc') == nil, 'a bot spawned over the server-wide cap')
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_TOTAL', 60)
    end)

    it('logs out when its play session ends, unless it is in a party', function()
        xi.test.world:setSetting('map.PLAYER_BOTS_STAY_MIN', 0)
        xi.test.world:setSetting('map.PLAYER_BOTS_STAY_MAX', 0)
        player:setLevel(10)
        local partyBot = inviteBot('Stayingbot')
        assert(partyBot:getPartySize() == 2, 'party bot did not join')
        assert(spawnBot('Leavingbot'))

        xi.test.world:skipTime(2)

        local names = {}
        for _, bot in ipairs(GetZone(zoneId):getBots()) do
            names[bot:getName()] = true
        end

        assert(not names.Leavingbot, 'a bot stayed past its play session')
        assert(names.Stayingbot, 'a party bot logged out in the middle of its party')
        xi.test.world:setSetting('map.PLAYER_BOTS_STAY_MIN', 30)
        xi.test.world:setSetting('map.PLAYER_BOTS_STAY_MAX', 120)
    end)

    it('aims for fewer bots in a city', function()
        -- Only the zone's id and type are read; the test world does not load cities.
        local city =
        {
            getID       = function()
                return xi.zone.SOUTHERN_SAN_DORIA
            end,

            getTypeMask = function()
                return xi.zoneType.CITY
            end,
        }
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_PER_ZONE', 12)
        local target = xi.bots.targetCount(city)
        assert(target >= 4 and target <= 6, string.format('a city aims for %d bots', target))
    end)

    it('swaps only head or body pieces the job can wear', function()
        for _ = 1, 300 do
            local job   = math.random(xi.job.WAR, xi.job.SMN)
            local level = math.random(1, 75)
            local base  = xi.bots.gearFor(job, level)
            local gear  = xi.bots.varyGear(job, level, base)
            for slot, itemId in pairs(gear) do
                if itemId ~= base[slot] then
                    assert(slot == xi.slot.HEAD or slot == xi.slot.BODY, string.format('slot %d was swapped', slot))
                    assert(CanBotWear(itemId, job, level, slot), string.format('job %d level %d got item %d', job, level, itemId))
                end
            end
        end
    end)

    -- Batch 6: GM tools

    it('lists live bots with what they are doing, for whois and the admin site', function()
        player:setLevel(10)
        local bot = inviteBot('Whoisbot')
        local found
        for _, info in ipairs(GetBotList()) do
            if info.name == 'Whoisbot' then
                found = info
            end
        end

        assert(found, 'GetBotList left out a live bot')
        assert(found.id == bot:getID() and found.zoneId == zoneId, 'GetBotList has the wrong id or zone')
        assert(found.state == 'party' and found.partyWith == player:getName(), 'GetBotList has the wrong state: ' .. found.state)
    end)

    it('stops arrivals and hunting while paused', function()
        SetBotsPaused(true)
        xi.test.world:setSetting('map.PLAYER_BOTS_MAX_PER_ZONE', 5)
        local mob = assert(findPrey(), 'no mob far enough from the player')
        local bot = spawnHunter('Pausebot', mob)
        for _ = 1, 30 do
            xi.test.world:skipTime(1)
            assert(not (bot:isEngaged() and bot:getTarget() and bot:getTarget():getID() == mob:getID()), 'a paused bot started a hunt')
        end

        assert(GetZone(zoneId):getBotCount() == 1, 'bots arrived while paused')
        SetBotsPaused(false)
    end)
end)
