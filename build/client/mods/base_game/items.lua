local path = ...

local function item(id, definition)
    midless.define_texture("base_game:" .. id, path .. "/textures/" .. id .. ".png")
    definition.texture = "base_game:" .. id
    midless.define_item("base_game:" .. id, definition)
end

item("stick", {name = "Stick"})
item("iron_ingot", {name = "Iron Ingot"})
item("charcoal", {name = "Charcoal"})
midless.define_item("base_game:coal", {name = "Coal", texture = "base_game:charcoal"})

midless.define_block("midless:coal_ore", {
    name = "Coal Ore", textures = {all = 7},
    hardness = 4, dig_group = "stone",
    drops = {{id = "base_game:coal", count = 1}},
})

for _, tier in ipairs({
    {id = "wooden", name = "Wooden", speed = 2, uses = 60, level = 1},
    {id = "stone", name = "Stone", speed = 4, uses = 132, level = 2},
    {id = "iron", name = "Iron", speed = 6, uses = 251, level = 3},
}) do
    for _, kind in ipairs({
        {id = "pickaxe", name = "Pickaxe", group = "stone"},
        {id = "axe", name = "Axe", group = "wood"},
        {id = "shovel", name = "Shovel", group = "soil"},
    }) do
        local id = tier.id .. "_" .. kind.id
        item(id, {
            name = tier.name .. " " .. kind.name,
            max_stack = 1,
            dig_speed = {[kind.group] = tier.speed},
            harvest_levels = {[kind.group] = tier.level},
            metadata = {{name = "durability", type = "uint", bits = 16, default = tier.uses}},
            on_dig = function(player, block, stack)
                stack.metadata.durability = stack.metadata.durability - 1
                if stack.metadata.durability <= 0 then
                    player:set_selected_stack(nil)
                else
                    player:set_selected_stack(stack)
                end
            end,
        })
    end
end
