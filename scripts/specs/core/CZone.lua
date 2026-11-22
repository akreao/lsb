---@meta

-- luacheck: ignore 241
---@class CZone
local CZone = {}

---@nodiscard
---@return integer
function CZone:getLocalVar(key)
end

---@nodiscard
---@return table
function CZone:getLocalVars()
end

---@param key string
---@param value integer
---@return nil
function CZone:setLocalVar(key, value)
end

---@return nil
function CZone:resetLocalVars()
end

---@return nil
function CZone:registerCuboidTriggerArea(triggerAreaID, xMin, yMin, zMin, xMax, yMax, zMax, rotation)
end

---@return nil
function CZone:registerCylindricalTriggerArea(triggerAreaID, xPos, zPos, radius)
end

---@return nil
function CZone:registerSphericalTriggerArea(triggerAreaID, xPos, yPos, zPos, radius)
end

---@deprecated
---@return nil
function CZone:levelRestriction()
end

---@nodiscard
---@return table
function CZone:getPlayers()
end

---@nodiscard
---@return table
function CZone:getNPCs()
end

---@nodiscard
---@return table
function CZone:getMobs()
end

---@nodiscard
---@return xi.zone
function CZone:getID()
end

---@nodiscard
---@return string
function CZone:getName()
end

---@nodiscard
---@return xi.region
function CZone:getRegionID()
end

---@nodiscard
---@return xi.zoneType
function CZone:getTypeMask()
end

---@nodiscard
---@param charID integer
---@return CBattlefield?
function CZone:getBattlefieldByInitiator(charID) -- Unused
end

---@nodiscard
---@return xi.weather
function CZone:getWeather()
end

---@nodiscard
---@return integer
function CZone:getUptime()
end

---@return nil
function CZone:reloadNavmesh()
end

---@param config table
---@return nil
function CZone:rebuildNavmesh(config)
end

---@nodiscard
---@param position table
---@return boolean
function CZone:isNavigablePoint(position)
end

---@nodiscard
---@param position table
---@return xi.terrain
function CZone:getTerrainType(position)
end

---@nodiscard
---@param position table
---@return integer
function CZone:getFloorId(position)
end

---@class dynamicEntityParams
---@field objtype? xi.objType            # Entity type. Default: xi.objType.NPC
---@field groupId? integer               # Mob group ID (for TYPE_MOB). Default: 0
---@field groupZoneId? integer           # Zone ID for mob group lookup (for TYPE_MOB). Default: 0
---@field allegiance? xi.allegiance      # Entity allegiance. Default: xi.allegiance.MOB
---@field name string                    # Entity name (required). Will be prefixed with "DE_" internally.
---@field packetName? string             # Display name sent in packets. Default: name
---@field x? number                      # X position. Default: 0.01
---@field y? number                      # Y position. Default: 0.01
---@field z? number                      # Z position. Default: 0.01
---@field rotation? integer              # Rotation (0-255). Default: 0
---@field look? integer|string           # Model ID (integer) or look string (e.g., "0x0000...")
---@field releaseIdOnDisappear? boolean  # Release target ID when entity disappears. Default: false
---@field entityFlags? integer           # Entity flags bitmask. Default: Uses Group/Pool ID Data
---@field namevis? integer               # (NPC) Name visibility flags. Default: 0
---@field widescan? integer              # (NPC) Widescan visibility. Default: 1
---@field onTrigger? function            # (NPC) onTrigger Function for Player Interaction
---@field mixins? table                  # (MOB) Table of mixins to apply.
---@field minLevel? integer              # (MOB) Minimum level override. Default: Uses Group ID Data
---@field maxLevel? integer              # (MOB) Maximum level override. Default: Uses Group ID Data
---@field dropId? integer                # (MOB) Drop table ID override. Default: Uses Group ID Data
---@field skillList? integer             # (MOB) Mob skill list ID override. Default: Uses Group ID Data
---@field spellList? integer             # (MOB) Spell list ID override. Default: Uses Group ID Data
---@field respawn? integer               # (MOB) Respawn time in seconds. 0 = no respawn.
---@field spawnType? integer             # (MOB) Spawn type flags.
---@field modelSize? integer             # (MOB) Model size override. Default: Uses Pool ID Data
---@field modelHitboxSize? integer       # (MOB) Hitbox size override. Default: Uses Pool ID Data
---@field isAggroable? boolean           # (MOB) Whether mob can aggro. Default: false
---@field specialSpawnAnimation? boolean # (MOB) Use special spawn animation. Default: false
---@field onMobDeath? function           # (MOB) Death Function. Default: function() end
---@param entityTable table<dynamicEntityParams>
---@return CBaseEntity?
function CZone:insertDynamicEntity(entityTable)
end

---@class botSpawnParams
---@field name string        # Bot name (memory only, never saved)
---@field race? integer      # xi.race. Default: 1
---@field face? integer      # Face id. Default: 0
---@field size? integer      # Model size. Default: 0
---@field job? integer       # Main job. Default: 1
---@field level? integer     # Main job level. Default: 1
---@field subJob? integer    # Support job. Default: 0 (none)
---@field subLevel? integer  # Support job level. Default: 0
---@field x? number
---@field y? number
---@field z? number
---@field rot? integer
---@field gear? table<integer, integer> # xi.slot -> item id
---@field seeking? boolean     # Shows the seeking party flag while on its own. Default: false

---@param botTable botSpawnParams
---@return CBaseEntity?
function CZone:spawnBot(botTable)
end

---@return nil
function CZone:despawnBots()
end

---@nodiscard
---@return integer
function CZone:getBotCount()
end

---@nodiscard
---@return CBaseEntity[]
function CZone:getBots()
end

-- Where players arrive in the zone (landing points of the zone lines leading in), for arriving bots.
---@nodiscard
---@return { x: number, y: number, z: number, rot: integer }[]
function CZone:getBotEntrances()
end

-- The height of the first floor (collision mesh) a ray meets going down from top to bottom at x, z. Down is +y.
---@nodiscard
---@param x number
---@param top number
---@param z number
---@param bottom number
---@return number?
function CZone:getGroundHeight(x, top, z, bottom)
end

---@nodiscard
---@return integer
function CZone:getSoloBattleMusic()
end

---@nodiscard
---@return integer
function CZone:getPartyBattleMusic()
end

---@nodiscard
---@return integer
function CZone:getBackgroundMusicDay()
end

---@nodiscard
---@return integer
function CZone:getBackgroundMusicNight()
end

---@param musicId integer
---@return nil
function CZone:setSoloBattleMusic(musicId)
end

---@param musicId integer
---@return nil
function CZone:setPartyBattleMusic(musicId)
end

---@param musicId integer
---@return nil
function CZone:setBackgroundMusicDay(musicId)
end

---@param musicId integer
---@return nil
function CZone:setBackgroundMusicNight(musicId)
end

---@nodiscard
---@param name string
---@return table
function CZone:queryEntitiesByName(name)
end
