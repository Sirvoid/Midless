local path = ...
local HULL_HEIGHT = 0.5

local function part(minimum, maximum)
    local x = math.min(16, maximum[1] - minimum[1])
    local y = math.min(16, maximum[2] - minimum[2])
    local z = math.min(16, maximum[3] - minimum[3])
    local uv = {
        east = {64, 0, z, y}, west = {64, 0, z, y},
        up = {64, 0, x, z}, down = {64, 0, x, z},
        north = {64, 0, x, y}, south = {64, 0, x, y},
    }
    return {role = model.part.NONE, position = {0, 0, 0}, min = minimum, max = maximum, uv = uv}
end
midless.define_entity_model("midless:boat", {
    name = "Boat", texture = "midless:terrain",
    parts = {
        part({-9, 0, -13}, {0, 2, 13}), part({0, 0, -13}, {9, 2, 13}),
        part({-9, 2, -13}, {-7, 8, 13}), part({7, 2, -13}, {9, 8, 13}),
        part({-7, 2, -13}, {7, 8, -11}), part({-7, 2, 11}, {7, 8, 13}),
        part({-7, 4, -2}, {7, 6, 2}),
    },
})

midless.define_entity("midless:boat", {
    model = "midless:boat",
    body = {enabled = true, gravity_scale = 1, ground_friction = 8,
        buoyancy = 2, liquid_drag = 0.7, liquid_vertical_drag = 6, liquid_lateral_drag = 4.3,
        min = {x = -0.5625, y = 0, z = -0.8125},
        max = {x = 0.5625, y = HULL_HEIGHT, z = 0.8125}},
    on_control = function(self, player, input, dt)
        if input.sneak then player:detach(); return end
        local wet = self.object:get_submerged_fraction() or 0
        if wet == 0 then return end -- Oars cannot propel a beached boat.
        local r = self.object:get_rotation()
        r.y = (r.y - input.sideways * 1.6 * wet * dt) % (2 * math.pi)
        self.object:set_rotation(r)
        local v = self.object:get_velocity()
        local x, z = math.sin(r.y), math.cos(r.y)
        local speed = v.x * x + v.z * z
        local target = input.forward * (input.forward < 0 and 2.5 or 6)
        local change = math.max(-8 * wet * dt, math.min(8 * wet * dt, target - speed))
        -- Releasing the keys lets water drag slow the boat naturally.
        if input.forward ~= 0 then
            v.x, v.z = v.x + x * change, v.z + z * change
            self.object:set_velocity(v)
        end
    end,
})

local function board(player, boat)
    if player:get_attachment() or boat:get_controller() then return end
    player:attach(boat, {offset = {x = 0, y = 0.375, z = 0}, inherit_rotation = false})
    boat:set_controller(player)
end

midless.define_texture("midless:boat", path .. "/textures/boat.png")
midless.define_item("midless:boat", {
    name = "Boat", texture = "midless:boat", max_stack = 1,
    on_use = function(player, stack)
        if player:get_attachment() then return true end
        local p, d = player:get_eye_position(), player:get_look_direction()
        local hit = midless.raycast(p, {x = p.x + d.x * 4.5, y = p.y + d.y * 4.5, z = p.z + d.z * 4.5},
            {liquids = true, ignore_player = player:get_id()})
        if hit.type == "entity" then
            if hit.entity:get_name() == "midless:boat" then board(player, hit.entity) end
            return true
        end
        if hit.type ~= "block" or hit.block:get_id() ~= "midless:water" or hit.normal.y <= 0 then
            return true
        end
        local position = {x = hit.position.x, y = hit.position.y - HULL_HEIGHT / 2, z = hit.position.z}
        local boat = midless.spawn_entity("midless:boat", position)
        if not boat:try_teleport(position, false, true) or (boat:get_submerged_fraction() or 0) == 0 then
            boat:remove()
        else
            boat:set_rotation({x = 0, y = math.atan(d.x, d.z), z = 0})
            stack.count = stack.count - 1
            if stack.count == 0 then player:set_selected_stack(nil)
            else player:set_selected_stack(stack) end
        end
        return true
    end,
})

local wood = "midless:wood"
midless.define_recipe({group = "crafting",
    pattern = {{wood, 0, wood}, {wood, wood, wood}},
    output = {id = "midless:boat", count = 1},
})

midless.register_on_player_click(function(player, button)
    if button ~= "right" or player:get_attachment() then return end
    local p, d = player:get_eye_position(), player:get_look_direction()
    local hit = midless.raycast(p, {x = p.x + d.x * 4, y = p.y + d.y * 4, z = p.z + d.z * 4},
        {ignore_player = player:get_id()})
    if hit.type == "entity" and hit.entity:get_name() == "midless:boat" then board(player, hit.entity) end
end)

midless.register_on_player_attack(function(player, target)
    if target.type ~= "entity" or target.entity:get_name() ~= "midless:boat" then return end
    local position = target.entity:get_position()
    position.y = position.y + HULL_HEIGHT
    if midless.spawn_item(position, {id = "midless:boat", count = 1}) then
        target.entity:remove()
    end
    return true
end)
