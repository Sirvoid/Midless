local STONE_STAIRS = "midless:stone_stairs"
local WOOD_STAIRS = "midless:wood_stairs"

local function place_facing(player, block)
    local look = player:get_look_direction()
    local facing = (math.abs(look.x) > math.abs(look.z))
        and (look.x > 0 and 3 or 1)
        or (look.z > 0 and 0 or 2)
    block:set_metadata("facing", facing)
end

local function stairs_boxes()
    return {
        {min = {0, 0, 0}, max = {16, 8, 16}},
        {min = {0, 8, 8}, max = {16, 16, 16}},
    }
end

midless.define_block(STONE_STAIRS, {
    name = "Stone Stairs",
    textures = {all = 1},
    boxes = stairs_boxes(),
    hardness = 3, dig_group = "stone",
    metadata = {{name = "facing", type = "uint", bits = 2, default = 0}},
    state_fields = {"facing"},
    variants = {
        {when = {}, rotate_y_from = "facing"},
    },
    on_place = function(player, block)
        place_facing(player, block)
    end,
})

midless.define_block(WOOD_STAIRS, {
    name = "Wood Stairs",
    textures = {all = 4},
    boxes = stairs_boxes(),
    hardness = 2, dig_group = "wood",
    metadata = {{name = "facing", type = "uint", bits = 2, default = 0}},
    state_fields = {"facing"},
    variants = {
        {when = {}, rotate_y_from = "facing"},
    },
    on_place = function(player, block)
        place_facing(player, block)
    end,
})
