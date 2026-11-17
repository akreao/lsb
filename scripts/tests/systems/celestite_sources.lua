-----------------------------------
-- Where celestite comes from: Stinknix (Lower Jeuno) sells it, Syndella registers it by trade.
-----------------------------------
describe('Celestite sources', function()
    ---@type CClientEntityPair
    local player

    before_each(function()
        player = xi.test.world:spawnPlayer({ zone = xi.zone.RULUDE_GARDENS })
        player:setLevel(99)
        player:addKeyItem(xi.keyItem.JOB_BREAKER)
        player:addKeyItem(xi.keyItem.CIPHER_BRACELET)
        player:addKeyItem(xi.keyItem.CELESTITE_CASE)
        player:addKeyItem(xi.keyItem.WINDURST_TRUST_PERMIT)
    end)

    it('registers a traded celestite and takes it', function()
        player:addItem(xi.item.CELESTITE_ATTACK)
        player.actions:tradeNpc('Syndella', { xi.item.CELESTITE_ATTACK }, { eventId = 10309, finishOption = 1 })

        assert(player:hasCelestite(xi.item.PIECE_OF_CELESTITE_ATTACK), 'celestite was not registered')
        player.assert.no:hasItem(xi.item.CELESTITE_ATTACK)
    end)

    it('registers only one of several celestite traded together', function()
        player:addItem(xi.item.CELESTITE_ATTACK)
        player:addItem(xi.item.CELESTITE_M_ATK)
        player.actions:tradeNpc('Syndella', { xi.item.CELESTITE_ATTACK, xi.item.CELESTITE_M_ATK }, { eventId = 10309, finishOption = 1 })

        local registered = (player:hasCelestite(xi.item.PIECE_OF_CELESTITE_ATTACK) and 1 or 0) +
            (player:hasCelestite(xi.item.PIECE_OF_CELESTITE_MAGIC_ATK) and 1 or 0)
        assert(registered == 1, string.format('expected one registered, saw %d', registered))

        local kept = (player:hasItem(xi.item.CELESTITE_ATTACK) and 1 or 0) + (player:hasItem(xi.item.CELESTITE_M_ATK) and 1 or 0)
        assert(kept == 1, string.format('expected one celestite item kept, saw %d', kept))
    end)

    it('keeps a celestite that is already registered', function()
        player:addCelestite(xi.item.PIECE_OF_CELESTITE_ATTACK)
        player:addItem(xi.item.CELESTITE_ATTACK)
        player.actions:tradeNpc('Syndella', { xi.item.CELESTITE_ATTACK }, { eventId = 10309, finishOption = 0 })

        player.assert:hasItem(xi.item.CELESTITE_ATTACK)
    end)

    it('is sold by Stinknix', function()
        player:gotoZone(xi.zone.LOWER_JEUNO)
        if player:isInEvent() then
            player.events:finish()
        end

        player:setGil(31740)
        player.entities:gotoAndTrigger('Stinknix')
        player.actions:shopBuy(7, 1)

        player.assert:hasItem(xi.item.CELESTITE_ACC)
        assert(player:getGil() == 0, string.format('expected 0 gil left, saw %d', player:getGil()))
    end)

    it('adds 20 alter ego points with a Trust Primer', function()
        player:setCurrency('alter_ego_points', 0)
        player:addItem(xi.item.TRUST_MAGIC_PRIMER)
        local primer = assert(player:findItem(xi.item.TRUST_MAGIC_PRIMER))
        player.actions:useItem(player, primer:getSlotID())
        xi.test.world:skipTime(5)

        assert(player:getCurrency('alter_ego_points') == 20,
            string.format('expected 20 points, saw %d', player:getCurrency('alter_ego_points')))
    end)
end)
