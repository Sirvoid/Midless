local WOOD_DOOR = "midless:wood_door"
local WOOD_DOOR_TOP = "midless:wood_door_top"

local function above(pos, offset)
    return {x = pos.x, y = pos.y + offset, z = pos.z}
end

local function partner(pos, id)
    return midless.get_block(above(pos, id == WOOD_DOOR and 1 or -1)),
        id == WOOD_DOOR and WOOD_DOOR_TOP or WOOD_DOOR
end

local function interact(player, door)
    local other, expected = partner(door:get_position(), door:get_id())
    if not other:is_loaded() or other:get_id() ~= expected then return end
    local open = not door:get_metadata("open")
    door:set_metadata("open", open)
    other:set_metadata("open", open)
end

local function place(player, door)
    local pos = door:get_position()
    local top = midless.get_block(above(pos, 1))
    local look = player:get_look_direction()
    local facing = math.abs(look.x) > math.abs(look.z)
        and (look.x > 0 and 1 or 3) or (look.z > 0 and 2 or 0)
    if top:is_loaded() and top:get_id() == "midless:air"
        and midless.set_block(above(pos, 1), WOOD_DOOR_TOP, {facing = facing, open = false}) then
        door:set_metadata("facing", facing)
        return
    end
    -- refund item if the top cannot be created.
    door:set_id("midless:air")
    local refund = {id = WOOD_DOOR, count = 1}
    if not player:get_inventory():add_item(refund) then
        midless.spawn_item(pos, refund)
    end
end

local function can_place(player, stack, target)
    if target.type ~= "block" then return true end
    local pos, normal = target.block:get_position(), target.normal
    local top = midless.get_block({x = pos.x + normal.x,
        y = pos.y + normal.y + 1, z = pos.z + normal.z})
    return not top:is_loaded() or top:get_id() ~= "midless:air"
end

local function check_partner(pos)
    local door = midless.get_block(pos)
    if not door:is_loaded() then return end
    local id = door:get_id()
    if id ~= WOOD_DOOR and id ~= WOOD_DOOR_TOP then return end
    local other, expected = partner(pos, id)
    if not other:is_loaded() then
        midless.schedule_block_update(pos, 0.5)
    elseif other:get_id() ~= expected then
        door:set_id("midless:air")
    end
end

local function definition(name)
    return {
        name = name,
        textures = {all = 4},
        hardness = 2, dig_group = "wood",
        boxes = {{min = {0, 0, 0}, max = {16, 16, 3}}},
        metadata = {
            {name = "facing", type = "uint", bits = 2, default = 0},
            {name = "open", type = "bool", default = false},
        },
        state_fields = {"facing", "open"},
        models = {
            closed = {boxes = {{min = {0, 0, 0}, max = {16, 16, 3}}}},
            open = {boxes = {{min = {0, 0, 0}, max = {3, 16, 16}}}},
        },
        variants = {
            {when = {open = false}, model = "closed", rotate_y_from = "facing"},
            {when = {open = true}, model = "open", rotate_y_from = "facing"},
        },
        drops = {WOOD_DOOR},
        on_interact = interact,
        physics = {interval = 0.05},
        on_physics = check_partner,
    }
end

local bottom = definition("Wooden Door")
bottom.on_use = can_place
bottom.on_place = place
midless.define_block(WOOD_DOOR, bottom)

local top = definition("Wooden Door (Top)")
top.on_use = function() return true end
midless.define_block(WOOD_DOOR_TOP, top)

midless.register_on_block_update(function(pos, id, previous)
    if id == previous or (previous ~= WOOD_DOOR and previous ~= WOOD_DOOR_TOP) then return end
    local other, expected = partner(pos, previous)
    if other:is_loaded() and other:get_id() == expected then
        other:set_id("midless:air")
    end
end)
