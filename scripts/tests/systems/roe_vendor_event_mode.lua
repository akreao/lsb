-----------------------------------
-- Rolandienne's events start with retail's Mode (0x032 @0x0E, 0x034 @0x2E): 0x1a, which hides other players and the NPCs
-- outside the event while it runs.
-----------------------------------
local ffi          = require('ffi')
local packetReader = require('scripts.tests.helpers.packet_reader')

local function lastMode(player)
    local mode
    for _, packet in ipairs(player.packets:getIncoming()) do
        if packet.type == 0x032 then
            mode = packetReader.readU16(packet.data, 0x0E)
        elseif packet.type == 0x034 then
            mode = packetReader.readU16(packet.data, 0x2E)
        end
    end

    assert(mode, 'No event start was sent')

    return mode
end

local function talk(player)
    local npc = player.entities:get('Rolandienne')
    player.entities:moveTo(npc)
    player.packets:clear()
    player.actions:trigger(npc)
end

describe('Rolandienne event mode', function()
    it('sends Mode 0x1a with the Records of Eminence introduction', function()
        local player = xi.test.world:spawnPlayer({ zone = xi.zone.SOUTHERN_SAN_DORIA })
        -- Records of Eminence record 1, taken with 0x10C as the client does.
        local record = ffi.new('uint8_t[8]')
        record[4] = 1
        player.packets:send(0x10C, record, assert(ffi.sizeof(record)))
        assert(player:getEminenceProgress(1) ~= nil, 'Record 1 is not active')

        talk(player)

        player.events:expect({ eventId = 993 })
        assert(lastMode(player) == 0x1a, string.format('Mode %#x', lastMode(player)))
    end)

    it('sends Mode 0x1a and the shop params with the spark exchange', function()
        local player = xi.test.world:spawnPlayer({ zone = xi.zone.SOUTHERN_SAN_DORIA })
        player:addKeyItem(xi.keyItem.MEMORANDOLL)
        player:addCurrency('spark_of_eminence', 1234)

        talk(player)

        player.events:expect({ eventId = 995 })
        assert(lastMode(player) == 0x1a, string.format('Mode %#x', lastMode(player)))
        local params = packetReader.lastEventStart(player).params
        assert(params[0] == 0, 'param 0 changed')
        assert(params[1] == 1234, string.format('sparks param %d', params[1]))
    end)
end)
