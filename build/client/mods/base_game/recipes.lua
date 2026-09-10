local wood, stone, stick = "midless:wood", "midless:stone", "midless:stick"
local stone_stairs, wood_stairs = "midless:stone_stairs", "midless:wood_stairs"
local function shaped(pattern, id, count)
    midless.define_recipe({group = "crafting", pattern = pattern,
        output = {id = id, count = count or 1}})
end
midless.define_recipe({group = "crafting", ingredients = {"midless:log"},
    output = {id = wood, count = 4}})
shaped({{wood}, {wood}}, stick, 4)
for _, fuel in ipairs({"midless:coal", "midless:charcoal"}) do
    shaped({{fuel}, {stick}}, "midless:torch", 4)
end
shaped({{wood, wood}, {wood, wood}}, "midless:crafting_table")
shaped({{wood, wood, wood}, {wood, 0, wood}, {wood, wood, wood}}, "midless:chest")
shaped({{stone, stone, stone}, {stone, 0, stone}, {stone, stone, stone}}, "midless:furnace")
shaped({{stone, 0, 0}, {stone, stone, 0}, {stone, stone, stone}}, stone_stairs, 4)
shaped({{wood, 0, 0}, {wood, wood, 0}, {wood, wood, wood}}, wood_stairs, 4)
for _, tier in ipairs({{"wooden", wood}, {"stone", stone}, {"iron", "midless:iron_ingot"}}) do
    local name, material = tier[1], tier[2]
    shaped({{material, material, material}, {0, stick, 0}, {0, stick, 0}}, "midless:" .. name .. "_pickaxe")
    shaped({{material, material}, {material, stick}, {0, stick}}, "midless:" .. name .. "_axe")
    shaped({{material, material}, {stick, material}, {stick, 0}}, "midless:" .. name .. "_axe")
    shaped({{material}, {stick}, {stick}}, "midless:" .. name .. "_shovel")
end
