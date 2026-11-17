-----------------------------------
-- ID: 6717
-- Item: Trust Tome
-- Grants 100 alter ego points (retail capture: action message 827, param 100).
-----------------------------------
---@type TItem
local itemObject = {}

itemObject.onItemCheck = function(target, item, caster)
    -- UNVERIFIED: the points are only of use with the Cipher Bracelet; retail's refusal without it was not captured.
    if not target:hasKeyItem(xi.keyItem.CIPHER_BRACELET) then
        return xi.msg.basic.ITEM_UNABLE_TO_USE
    end

    return 0
end

itemObject.onItemUse = function(target, user, item, action)
    target:addCurrency('alter_ego_points', 100)
    action:messageID(target:getID(), xi.msg.basic.USE_ALTER_EGO_POINTS)

    return 100
end

return itemObject
