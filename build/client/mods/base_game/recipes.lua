local wood, stone, stick = "midless:wood", "midless:stone", "base_game:stick"
local stone_stairs, wood_stairs = "base_game:stone_stairs", "base_game:wood_stairs"
local function shaped(pattern, id, count)
    midless.define_recipe({group = "crafting", pattern = pattern,
        output = {id = id, count = count or 1}})
end
midless.define_recipe({group = "crafting", ingredients = {"midless:log"},
    output = {id = wood, count = 4}})
shaped({{wood}, {wood}}, stick, 4)
for _, fuel in ipairs({"base_game:coal", "base_game:charcoal"}) do
    shaped({{fuel}, {stick}}, "base_game:torch", 4)
end
shaped({{wood, wood}, {wood, wood}}, "base_game:crafting_table")
shaped({{wood, wood, wood}, {wood, 0, wood}, {wood, wood, wood}}, "base_game:chest")
shaped({{stone, stone, stone}, {stone, 0, stone}, {stone, stone, stone}}, "base_game:furnace")
shaped({{stone, 0, 0}, {stone, stone, 0}, {stone, stone, stone}}, stone_stairs, 4)
shaped({{wood, 0, 0}, {wood, wood, 0}, {wood, wood, wood}}, wood_stairs, 4)
for _, tier in ipairs({{"wooden", wood}, {"stone", stone}, {"iron", "base_game:iron_ingot"}}) do
    local name, material = tier[1], tier[2]
    shaped({{material, material, material}, {0, stick, 0}, {0, stick, 0}}, "base_game:" .. name .. "_pickaxe")
    shaped({{material, material}, {material, stick}, {0, stick}}, "base_game:" .. name .. "_axe")
    shaped({{material, material}, {stick, material}, {stick, 0}}, "base_game:" .. name .. "_axe")
    shaped({{material}, {stick}, {stick}}, "base_game:" .. name .. "_shovel")
end
