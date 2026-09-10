midless.define_block("base_game:torch", {
    name = "Torch",
    textures = {all = 21},
    boxes = {
        {min = {7, 0, 7}, max = {9, 10, 9}},
    },
    collider = block.collider.NONE,
    light = block.light.EMIT,
    hardness = 0,
})
