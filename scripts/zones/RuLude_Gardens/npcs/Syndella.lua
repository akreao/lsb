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
