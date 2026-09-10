local FURNACE = "base_game:furnace"
local INPUT, FUEL, OUTPUT = 1, 2, 3
local recipes = {
    ["midless:sand"] = {id = "midless:glass", count = 1, time = 10, key = 1}, -- All recipes take ten seconds.
    ["midless:iron_ore"] = {id = "base_game:iron_ingot", count = 1, time = 10, key = 2},
    ["midless:log"] = {id = "base_game:charcoal", count = 1, time = 10, key = 3},
}
local fuels = {["midless:wood"] = 10, ["midless:log"] = 15,
    ["base_game:stick"] = 5, ["base_game:charcoal"] = 80, ["base_game:coal"] = 80} -- burn time in seconds

local function keys(values)
    local result = {}
    for id in pairs(values) do result[#result + 1] = id end
    return result
end

local function can_cook(items, recipe)
    if not recipe then return false end
    local output = items:get_stack(OUTPUT)
    return not output or (output.id == recipe.id and output.count + recipe.count <= 64)
end

local function wake(block)
    if block:timer_started() then return end
    local items = block:get_inventory("items")
    local input, fuel = items:get_stack(INPUT), items:get_stack(FUEL)
    if block:get_metadata("burn_left") > 0 or
        (input and can_cook(items, recipes[input.id]) and fuel and fuels[fuel.id]) then
        block:start_timer(1)
    end
end

midless.define_block(FURNACE, {
    name = "Furnace",
    textures = {all = 1, front = 18},
    hardness = 3.5, dig_group = "stone",
    metadata = {
        {name = "items", type = "inventory", slots = 3, rules = {
            [INPUT] = {items = keys(recipes)},
            [FUEL] = {items = keys(fuels)},
            [OUTPUT] = {insert = false},
        }},
        {name = "burn_left", type = "float"},
        {name = "burn_total", type = "float"},
        {name = "burn_progress", type = "float"},
        {name = "cook_progress", type = "float"},
        {name = "cook_item", type = "uint"},
        {name = "facing", type = "uint", default = 0},
    },
    state_fields = {"facing"},
    variants = {
        {when = {}, rotate_y_from = "facing"},
    },
    on_place = function(player, block)
        local look = player:get_look_direction()
        local facing
        -- The unrotated front is +Z. Point it back toward the player.
        if math.abs(look.x) > math.abs(look.z) then
            facing = look.x > 0 and 1 or 3
        else
            facing = look.z > 0 and 2 or 0
        end
        block:set_metadata("facing", facing)
    end,
    on_inventory_changed = function(block, field)
        if field ~= "items" then return end
        local input = block:get_inventory("items"):get_stack(INPUT)
        local id = input and recipes[input.id] and recipes[input.id].key or 0
        if block:get_metadata("cook_item") ~= id then
            block:set_metadata("cook_item", id)
            block:set_metadata("cook_progress", 0)
        end
        wake(block)
    end,
    on_timer = function(block, dt)
        local items = block:get_inventory("items")
        local input = items:get_stack(INPUT)
        local recipe = input and recipes[input.id]
        local progress = block:get_metadata("cook_progress")
        local id = input and recipes[input.id] and recipes[input.id].key or 0
        if block:get_metadata("cook_item") ~= id then progress = 0 end
        block:set_metadata("cook_item", id)

        local burn = block:get_metadata("burn_left")
        local ready = can_cook(items, recipe)
        if burn <= 0 and ready then
            local fuel = items:get_stack(FUEL)
            if fuel and fuels[fuel.id] and block:inventory_transaction("items", {
                take = {{slot = FUEL, id = fuel.id, count = 1}},
            }) then
                burn = fuels[fuel.id]
                block:set_metadata("burn_total", burn)
            end
        end

        local elapsed = math.min(dt, burn)
        burn = math.max(0, burn - dt)
        if ready then
            progress = progress + elapsed
            if progress >= recipe.time and block:inventory_transaction("items", {
                take = {{slot = INPUT, id = input.id, count = 1}},
                give = {{slot = OUTPUT, id = recipe.id, count = recipe.count}},
            }) then
                progress = progress - recipe.time
                if not items:get_stack(INPUT) then progress = 0 end
            end
        elseif not recipe then progress = 0 end

        block:set_metadata("burn_left", burn)
        block:set_metadata("burn_progress", burn / math.max(1, block:get_metadata("burn_total")))
        block:set_metadata("cook_progress", progress)
        if burn > 0 then return true end
        local fuel = items:get_stack(FUEL)
        input = items:get_stack(INPUT)
        return input and can_cook(items, recipes[input.id]) and fuel and fuels[fuel.id] ~= nil
    end,
    on_interact = function(player, block)
        local items = block:get_inventory("items")
        player:show_inventory({
            title = "Furnace", block = block, width = 9, height = 8,
            elements = {
                {type = "inventory", inventory = items, slot = INPUT, columns = 1, rows = 1, x = 2, y = 0},
                {type = "inventory", inventory = items, slot = FUEL, columns = 1, rows = 1, x = 2, y = 2},
                {type = "inventory", inventory = items, slot = OUTPUT, columns = 1, rows = 1, x = 6, y = 1},
                {type = "progress", value = "cook_progress", max = 10, x = 3.5, y = 1.25, width = 2, height = 0.5},
                {type = "progress", value = "burn_progress", max = 1, x = 2, y = 1.25, width = 1, height = 0.5},
                {type = "label", text = "Inventory", x = 0, y = 3.5},
                {type = "inventory", inventory = player:get_inventory(), columns = 9, rows = 4, x = 0, y = 4},
            },
        })
    end,
})

