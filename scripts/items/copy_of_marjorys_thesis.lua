-----------------------------------
-- ID: 6718
-- Item: Marjory's Thesis
-- Grants 200 alter ego points.
-- UNVERIFIED: the amount is from BG Wiki (Category:Trust, Alter Ego Points); the message (827) and the Cipher
-- Bracelet check follow the captured Trust Tome.
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
    target:addCurrency('alter_ego_points', 200)
    action:messageID(target:getID(), xi.msg.basic.USE_ALTER_EGO_POINTS)

    return 200
end

return itemObject
