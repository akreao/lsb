-----------------------------------
-- Monthly "Deeds" records that pay 10 deeds of heroism and, once alter ego points are unlocked, a Trust Primer.
-----------------------------------
local ffi = require('ffi')

describe('Monthly Deeds records', function()
    ---@type CClientEntityPair
    local player

    local function take(record)
        local packet = ffi.new('uint8_t[8]')
        packet[0x04] = record % 0x100
        packet[0x05] = math.floor(record / 0x100)
        player.packets:send(0x10C, packet, ffi.sizeof(packet))
        assert(player:hasEminenceRecord(record), string.format('record %d was not taken', record))
    end

    before_each(function()
        player = xi.test.world:spawnPlayer({ zone = xi.zone.RULUDE_GARDENS })
        player:setLevel(99)
        player:setEminenceCompleted(1) -- First Step Forward
        player:setCurrency('deeds', 0)
    end)

    it('completes Gobbie Mystery Box after 3 special dial spins with deeds and a Trust Primer', function()
        player:addKeyItem(xi.keyItem.CIPHER_BRACELET)
        take(3779)

        for _ = 1, 3 do
            xi.gobbieMysteryBox.countSpecialDial(player)
        end

        assert(player:getEminenceCompleted(3779), 'record 3779 was not completed')
        assert(player:getCurrency('deeds') == 10, string.format('expected 10 deeds, saw %d', player:getCurrency('deeds')))
        player.assert:hasItem(xi.item.TRUST_MAGIC_PRIMER)
    end)

    it('gives the Trust Primer before alter ego points are unlocked, but it cannot be used', function()
        take(3782)
        xi.roe.onRecordTrigger(player, 3782)

        assert(player:getEminenceCompleted(3782), 'record 3782 was not completed')
        assert(player:getCurrency('deeds') == 10, string.format('expected 10 deeds, saw %d', player:getCurrency('deeds')))
        player.assert:hasItem(xi.item.TRUST_MAGIC_PRIMER)

        player:gotoZone(xi.zone.RULUDE_GARDENS)
        local primer = assert(player:findItem(xi.item.TRUST_MAGIC_PRIMER))
        player.actions:useItem(player, primer:getSlotID())
        xi.test.world:skipTime(5)
        player.assert:hasItem(xi.item.TRUST_MAGIC_PRIMER)
        assert(player:getCurrency('alter_ego_points') == 0, 'the Primer was used without the Cipher Bracelet')
    end)
end)
