-----------------------------------
-- Trust Ascension: alter ego points (s2c 0x08E, c2s 0x0C1), celestite (s2c 0x08F, c2s 0x0C5) and the
-- Job Breaker / Syndella unlock chain, checked against a retail capture.
-----------------------------------
local ffi = require('ffi')
local packetReader = require('scripts.tests.helpers.packet_reader')

local hp = 8 -- AlterEgoCategory::HP

local function lastPacket(player, id)
    local found
    for _, packet in ipairs(player.packets:getIncoming()) do
        if packet.type == id then
            found = packet
        end
    end

    return found
end

local function countPackets(player, id)
    local count = 0
    for _, packet in ipairs(player.packets:getIncoming()) do
        if packet.type == id then
            count = count + 1
        end
    end

    return count
end

-- s2c 0x08E: points u16 @4, levels u8[32] @8, next costs u16[32] @0x28.
local function readPoints(player)
    local packet = assert(lastPacket(player, 0x08E), 'no 0x08E was sent')
    local levels = {}
    local costs  = {}
    for i = 0, 31 do
        levels[i] = packet.data[0x08 + i]
        costs[i]  = packetReader.readU16(packet.data, 0x28 + i * 2)
    end

    return packetReader.readU16(packet.data, 0x04), levels, costs
end

-- s2c 0x08F: owned bits @4, then 7 records of 20 bytes @0x24 (ten slot levels, ten celestites).
local function readCelestite(player)
    local packet = assert(lastPacket(player, 0x08F), 'no 0x08F was sent')
    local sets   = {}
    for set = 0, 6 do
        sets[set] = { levels = {}, worn = {} }
        for slot = 0, 9 do
            sets[set].levels[slot] = packet.data[0x24 + set * 20 + slot]
            sets[set].worn[slot]   = packet.data[0x24 + set * 20 + 10 + slot]
        end
    end

    return packetReader.readU32(packet.data, 0x04), sets
end

local function upgrade(player, category)
    local packet = ffi.new('uint8_t[8]')
    packet[4]    = category
    player.packets:send(0x0C1, packet, ffi.sizeof(packet))
end

local function setCelestite(player, set, slots)
    local packet = ffi.new('uint8_t[16]')
    packet[4]    = set
    for i, n in ipairs(slots) do
        packet[4 + i] = n
    end

    player.packets:send(0x0C5, packet, ffi.sizeof(packet))
end

describe('Trust Ascension', function()
    ---@type CClientEntityPair
    local player

    describe('alter ego points', function()
        before_each(function()
            player = xi.test.world:spawnPlayer({ zone = xi.zone.RULUDE_GARDENS })
            player:setLevel(99)
            player:addKeyItem(xi.keyItem.CIPHER_BRACELET)
            player:gotoMogHouse(xi.zone.RULUDE_GARDENS)
            if player:isInEvent() then
                player.events:finish()
            end
        end)

        it('sends the next level costs of attributes 8..18 with no upgrades', function()
            player.packets:clear()
            player:addCurrency('alter_ego_points', 0)

            local _, levels, costs = readPoints(player)
            for i = 0, 31 do
                local expected = (i >= 8 and i <= 18) and 1 or 0
                assert(costs[i] == expected, string.format('cost %d: expected %d, saw %d', i, expected, costs[i]))
                assert(levels[i] == 0, string.format('level %d: expected 0, saw %d', i, levels[i]))
            end
        end)

        it('charges 1 point up to level 10, then 2, and answers with message 828', function()
            player:setCurrency('alter_ego_points', 200)

            for _ = 1, 10 do
                upgrade(player, hp)
            end

            player.packets:clear()
            upgrade(player, hp)

            local points, levels, costs = readPoints(player)
            assert(levels[hp] == 11, string.format('expected level 11, saw %d', levels[hp]))
            assert(points == 188, string.format('expected 188 points, saw %d', points))
            assert(costs[hp] == 2, string.format('expected next cost 2, saw %d', costs[hp]))

            local message = assert(lastPacket(player, 0x029), 'no 0x029 was sent')
            assert(packetReader.readU16(message.data, 0x18) == 828, 'expected message 828')
            assert(packetReader.readU32(message.data, 0x0C) == hp, 'expected the attribute in Data')
            assert(packetReader.readU32(message.data, 0x10) == 11, 'expected the new level in Data2')
        end)

        it('stops at level 60 after 210 points', function()
            player:setCurrency('alter_ego_points', 300)

            for _ = 1, 61 do
                upgrade(player, hp)
            end

            local points, levels, costs = readPoints(player)
            assert(levels[hp] == 60, string.format('expected level 60, saw %d', levels[hp]))
            assert(points == 90, string.format('expected 90 points, saw %d', points))
            assert(costs[hp] == 0, string.format('expected no cost at the cap, saw %d', costs[hp]))
        end)

        it('refuses an upgrade without enough points', function()
            player:setCurrency('alter_ego_points', 0)
            upgrade(player, hp)

            assert(player:getCurrency('alter_ego_points') == 0)
            player.packets:clear()
            player:addCurrency('alter_ego_points', 0)
            local _, levels = readPoints(player)
            assert(levels[hp] == 0, 'level rose without points')
        end)

        it('adds 100 points with a Trust Tome', function()
            -- Retail used the tomes in town; LSB refuses item use in the Mog House.
            player:gotoZone(xi.zone.RULUDE_GARDENS)
            player:setCurrency('alter_ego_points', 0)
            player:addItem(xi.item.TRUST_MAGIC_TOME)
            local tome = assert(player:findItem(xi.item.TRUST_MAGIC_TOME))
            player.actions:useItem(player, tome:getSlotID())
            xi.test.world:skipTime(5)

            assert(player:getCurrency('alter_ego_points') == 100,
                string.format('expected 100 points, saw %d', player:getCurrency('alter_ego_points')))
            player.assert.no:hasItem(xi.item.TRUST_MAGIC_TOME)
        end)
    end)

    describe('celestite', function()
        before_each(function()
            player = xi.test.world:spawnPlayer({ zone = xi.zone.RULUDE_GARDENS })
            player:setLevel(99)
            player:addKeyItem(xi.keyItem.CELESTITE_CASE)
            player:addCelestite(xi.item.PIECE_OF_CELESTITE_DEFENSE)
            player:setCelestiteSlotLevel(1, 1)
            player:gotoMogHouse(xi.zone.RULUDE_GARDENS)
            if player:isInEvent() then
                player.events:finish()
            end
        end)

        it('answers 0x0C6 with 0x08F', function()
            player.packets:clear()
            local packet = ffi.new('uint8_t[4]')
            player.packets:send(0x0C6, packet, ffi.sizeof(packet))

            assert(countPackets(player, 0x08F) == 1, 'expected one 0x08F')
        end)

        it('wears a celestite in every set but once per set', function()
            for set = 1, 6 do
                player.packets:clear()
                setCelestite(player, set, { 4 })
                assert(countPackets(player, 0x08F) == 1, 'expected one 0x08F per 0x0C5')
            end

            local owned, sets = readCelestite(player)
            assert(owned == 0x10, string.format('expected owned bits 0x10, saw 0x%x', owned))
            for set = 1, 6 do
                assert(sets[set].levels[0] == 1, 'expected slot 1 at level 1')
                assert(sets[set].worn[0] == 4, string.format('set %d does not wear celestite 4', set))
            end

            assert(sets[0].worn[0] == 0, 'record 0 must stay empty')
        end)

        it('refuses a closed slot or a celestite that is not registered', function()
            setCelestite(player, 1, { 0, 4 })
            local _, sets = readCelestite(player)
            assert(sets[1].worn[1] == 0, 'celestite placed in a closed slot')

            setCelestite(player, 1, { 3 })
            _, sets = readCelestite(player)
            assert(sets[1].worn[0] == 0, 'unregistered celestite placed')
        end)
    end)

    describe('Job Breaker and Syndella', function()
        before_each(function()
            player = xi.test.world:spawnPlayer({ zone = xi.zone.RULUDE_GARDENS })
            player:setLevel(99)
            player:addKeyItem(xi.keyItem.LIMIT_BREAKER)
            player:addKeyItem(xi.keyItem.WINDURST_TRUST_PERMIT)
            player:completeQuest(xi.questLog.JEUNO, xi.quest.id.jeuno.BEYOND_INFINITY)
        end)

        it('undertakes the growth objective with Job Breaker', function()
            player.packets:clear()
            player.entities:gotoAndTrigger('Nomad_Moogle', { eventId = 10240, finishOption = 28 })

            local start = packetReader.lastEventStart(player)
            assert(start.params[0] == 99 and start.params[1] == 1 and start.params[2] == 5 and start.params[3] == 2,
                'unexpected 10240 params')

            player.assert:hasKI(xi.keyItem.JOB_BREAKER)
            assert(player:hasEminenceRecord(1067), 'record 1067 was not undertaken')

            local list = assert(lastPacket(player, 0x111), 'no 0x111 was sent')
            assert(packetReader.readU32(list.data, 0x04 + 42 * 4) % 0x1000 == 1067, 'record 1067 is not in entry 42')
        end)

        it('gives the Cipher Bracelet, a Trust Tome and completes the objective', function()
            player.entities:gotoAndTrigger('Nomad_Moogle', { eventId = 10240, finishOption = 28 })
            player.entities:gotoAndTrigger('Syndella', { eventId = 10300, finishOption = 1 })

            player.assert:hasKI(xi.keyItem.CIPHER_BRACELET)
            assert(player:getEminenceCompleted(1067), 'record 1067 was not completed')
            assert(not player:hasEminenceRecord(1067), 'record 1067 is still undertaken')

            local tomes = player:getItemCount(xi.item.TRUST_MAGIC_TOME)
            assert(tomes == 2, string.format('expected 2 Trust Tomes, saw %d', tomes))
        end)

        it('declines on option 2', function()
            player:addKeyItem(xi.keyItem.JOB_BREAKER)
            player.entities:gotoAndTrigger('Syndella', { eventId = 10300, finishOption = 2 })

            player.assert.no:hasKI(xi.keyItem.CIPHER_BRACELET)
        end)

        it('talks with 10302 without Job Breaker', function()
            player.entities:gotoAndTrigger('Syndella', { eventId = 10302, finishOption = 0 })
        end)

        it('gives the Celestite Case and Celestite (Defense) in slot 1 of every set', function()
            player:addKeyItem(xi.keyItem.JOB_BREAKER)
            player:addKeyItem(xi.keyItem.CIPHER_BRACELET)
            player.packets:clear()
            player.entities:gotoAndTrigger('Syndella', { eventId = 10308, finishOption = 1 })

            player.assert:hasKI(xi.keyItem.CELESTITE_CASE)
            assert(player:hasCelestite(xi.item.PIECE_OF_CELESTITE_DEFENSE), 'celestite not registered')

            local owned, sets = readCelestite(player)
            assert(owned == 0x10, string.format('expected owned bits 0x10, saw 0x%x', owned))
            for set = 1, 6 do
                assert(sets[set].levels[0] == 1 and sets[set].levels[1] == 0, 'expected one slot at level 1')
            end
        end)
    end)
end)
