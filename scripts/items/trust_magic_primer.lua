-----------------------------------
-- ID: 6716
-- Item: Trust Primer
-- Grants 20 alter ego points.
-- Wiki-sourced (BG Wiki, Trust Primer): "Consuming this gives the player 20 alter ego points. Must first obtain a Key
-- Item Cipher bracelet from Syndella". The Primer itself can be obtained before that (monthly objectives).
-- UNVERIFIED: the message (827) follows the captured Trust Tome; retail's refusal message was not captured.
-----------------------------------
---@type TItem
local itemObject = {}

itemObject.onItemCheck = function(target, item, caster)
    if not target:hasKeyItem(xi.keyItem.CIPHER_BRACELET) then
        return xi.msg.basic.ITEM_UNABLE_TO_USE
    end

    return 0
end

itemObject.onItemUse = function(target, user, item, action)
    target:addCurrency('alter_ego_points', 20)
    action:messageID(target:getID(), xi.msg.basic.USE_ALTER_EGO_POINTS)

    return 20
end

return itemObject
