-----------------------------------
-- Area: Ru'Lude Gardens
--  NPC: Syndella
-- Trust Ascension: Cipher Bracelet (alter ego points) and Celestite Case.
-- Events and rewards follow a retail capture (11/2026) unless marked otherwise.
-----------------------------------
local ID = zones[xi.zone.RULUDE_GARDENS]
-----------------------------------
---@type TNpcEntity
local entity = {}

-- Celestite registered by trade (retail capture): one Celestite item 0x7800+n becomes registered celestite 0x72F1+n
-- at once (0x08F before the event), then event 10309 with [registered id, 1]; option 1 shows 15793.
local celestiteRegistered =
{
    [xi.item.CELESTITE_ACC    ] = xi.item.PIECE_OF_CELESTITE_ACCURACY,
    [xi.item.CELESTITE_EVASION] = xi.item.PIECE_OF_CELESTITE_EVASION,
    [xi.item.CELESTITE_ATTACK ] = xi.item.PIECE_OF_CELESTITE_ATTACK,
    [xi.item.CELESTITE_M_ATK  ] = xi.item.PIECE_OF_CELESTITE_MAGIC_ATK,
    [xi.item.CELESTITE_M_DEF  ] = xi.item.PIECE_OF_CELESTITE_MAGIC_DEF,
}

entity.onTrade = function(player, npc, trade)
    if not player:hasKeyItem(xi.keyItem.CELESTITE_CASE) then
        return
    end

    -- November 2026 update notes: "Even if you trade multiple celestite items at the same time, only one can be
    -- registered within the UI per trade." UNVERIFIED which one: the first celestite in the trade window is taken
    -- and the rest stay with the player. Retail was captured with single trades only.
    local itemId     = 0
    local registered = nil
    for slot = 0, trade:getSlotCount() - 1 do
        local id = trade:getItemId(slot)
        if celestiteRegistered[id] then
            itemId     = id
            registered = celestiteRegistered[id]
            break
        end
    end

    if not registered then
        return
    end

    -- UNVERIFIED: an already registered celestite was not traded in the capture. The event script answers param 1
    -- = 2 with "you already have one affixed"; the item is kept.
    if player:hasCelestite(registered) then
        player:startEvent(10309, registered, 2)
        return
    end

    trade:confirmItem(itemId, 1)
    player:confirmTrade()
    player:addCelestite(registered)
    player:sendCelestite()
    player:setLocalVar('SyndellaCelestite', registered)
    player:startEvent(10309, registered, 1)
end

entity.onTrigger = function(player, npc)
    -- UNVERIFIED condition: 10301 says the player cannot use Trust magic yet; not captured.
    if not xi.trust.hasPermit(player) then
        player:startEvent(10301)

    elseif not player:hasKeyItem(xi.keyItem.JOB_BREAKER) then
        player:startEvent(10302)

    elseif not player:hasKeyItem(xi.keyItem.CIPHER_BRACELET) then
        player:startEvent(10300)

    elseif not player:hasKeyItem(xi.keyItem.CELESTITE_CASE) then
        player:startEvent(10308)

    else
        -- UNVERIFIED: 10303 ("I am able to observe your efforts through the bracelet") was not captured.
        player:startEvent(10303)
    end
end

entity.onEventFinish = function(player, csid, option, npc)
    if csid == 10300 and option == 1 then
        npcUtil.giveKeyItem(player, xi.keyItem.CIPHER_BRACELET)
        npcUtil.giveItem(player, xi.item.TRUST_MAGIC_TOME)

        -- Completes the growth objective undertaken with Job Breaker (messages 690, 707, 371, 693).
        if player:hasEminenceRecord(1067) then
            xi.roe.onRecordTrigger(player, 1067)
        end

    elseif csid == 10309 then
        local registered = player:getLocalVar('SyndellaCelestite')
        player:setLocalVar('SyndellaCelestite', 0)
        if option == 1 and registered ~= 0 then
            player:messageSpecial(ID.text.CELESTITE_CAN_BE_AFFIXED, registered, xi.keyItem.CIPHER_BRACELET)
        end

    elseif csid == 10308 and option == 1 then
        npcUtil.giveKeyItem(player, xi.keyItem.CELESTITE_CASE)

        -- Registers Celestite (Defense) and opens slot 1 at level 1 in all six sets.
        player:addCelestite(xi.item.PIECE_OF_CELESTITE_DEFENSE)
        player:setCelestiteSlotLevel(1, 1)
        player:messageSpecial(ID.text.CELESTITE_CAN_BE_AFFIXED, xi.item.PIECE_OF_CELESTITE_DEFENSE, xi.keyItem.CIPHER_BRACELET)
        player:messageSpecial(ID.text.NEW_ROE_OBJECTIVE_AVAILABLE)
        player:sendCelestite()
    end
end

return entity
