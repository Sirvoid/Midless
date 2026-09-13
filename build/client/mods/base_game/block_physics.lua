midless.define_block("midless:water", {
    name = "Water",
    textures = {all = 14},
    hardness = 0, dig_group = "liquid",
    physics = {type = "fluid", interval = 0.20, max_level = 7, renewable_sources = true},
})

midless.define_block("midless:sand", {
    name = "Sand",
    textures = {all = 11},
    hardness = 0.5, dig_group = "soil",
    physics = {type = "falling", interval = 0.1},
})
