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
        player = xi.test.world:spawnPlayer({ zone = zoneId })
        player.packets:clear()
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
            if packet.type == 0x00D and packetReader.readU32(packet.data, 0x04) == bot:getID() and not hpp then
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

    it('leaves when the last real player leaves the zone', function()
        assert(spawnBot('Leavebot'))
        assert(GetZone(zoneId):getBotCount() == 1, 'bot not counted')

        player:gotoZone(xi.zone.EAST_RONFAURE)

        assert(GetZone(zoneId):getBotCount() == 0, 'bot stayed in an empty zone')
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
