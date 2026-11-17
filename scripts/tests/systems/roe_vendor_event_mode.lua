-----------------------------------
-- The spark exchange NPCs' events start with retail's Mode (0x032 @0x0E, 0x034 @0x2E): 0x1a, which hides other players and the NPCs
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

local vendors =
{
    { name = 'Rolandienne',    zone = xi.zone.SOUTHERN_SAN_DORIA, intro = 993, exchange = 995 },
    { name = 'Isakoth',        zone = xi.zone.BASTOK_MARKETS,     intro = 24,  exchange = 26,  zoneIn = 22  },
    { name = 'Fhelm_Jobeizat', zone = xi.zone.WINDURST_WOODS,     intro = 848, exchange = 850, zoneIn = 839 },
    { name = 'Eternal_Flame',  zone = xi.zone.WESTERN_ADOULIN,    intro = 5079, exchange = 5081 },
}

-- A test character in Bastok Markets or Windurst Woods arrives in a Mog House exit event; expect() finishes it.
local function spawn(vendor)
    local player = xi.test.world:spawnPlayer({ zone = vendor.zone })
    if vendor.zoneIn then
        player.events:expect({ eventId = vendor.zoneIn })
    end

    return player
end

local function talk(player, name)
    local npc = player.entities:get(name)
    player.entities:moveTo(npc)
    player.packets:clear()
    player.actions:trigger(npc)
end

describe('Spark exchange event mode', function()
    for _, vendor in ipairs(vendors) do
        it(vendor.name .. ' sends Mode 0x1a with the Records of Eminence introduction', function()
            local player = spawn(vendor)
            -- Records of Eminence record 1, taken with 0x10C as the client does.
            local record = ffi.new('uint8_t[8]')
            record[4] = 1
            player.packets:send(0x10C, record, assert(ffi.sizeof(record)))
            assert(player:getEminenceProgress(1) ~= nil, 'Record 1 is not active')

            talk(player, vendor.name)

            player.events:expect({ eventId = vendor.intro })
            assert(lastMode(player) == 0x1a, string.format('Mode %#x', lastMode(player)))
        end)

        it(vendor.name .. ' sends Mode 0x1a and the shop params with the spark exchange', function()
            local player = spawn(vendor)
            player:addKeyItem(xi.keyItem.MEMORANDOLL)
            player:addCurrency('spark_of_eminence', 1234)

            talk(player, vendor.name)

            player.events:expect({ eventId = vendor.exchange })
            assert(lastMode(player) == 0x1a, string.format('Mode %#x', lastMode(player)))
            local params = packetReader.lastEventStart(player).params
            assert(params[0] == 0, 'param 0 changed')
            assert(params[1] == 1234, string.format('sparks param %d', params[1]))
        end)
    end

    it('Fhelm_Jobeizat keeps the gil param with the introduction', function()
        local player = spawn(vendors[3])
        local record = ffi.new('uint8_t[8]')
        record[4] = 1
        player.packets:send(0x10C, record, assert(ffi.sizeof(record)))
        player:setGil(4321)

        talk(player, 'Fhelm_Jobeizat')

        player.events:expect({ eventId = 848 })
        local params = packetReader.lastEventStart(player).params
        assert(params[0] == 0 and params[1] == 4321, string.format('params %d, %d', params[0], params[1]))
    end)
end)
