local path = ...
assert(type(path) == "string", "Load base_game as a mod folder")

midless.define_texture("midless:terrain", path .. "/textures/terrain.png")
midless.set_terrain_texture("midless:terrain")

assert(loadfile(path .. "/health.lua"))(path)

assert(loadfile(path .. "/items.lua"))(path)
dofile(path .. "/classic_terrain.lua")
dofile(path .. "/containers.lua")
dofile(path .. "/furnace.lua")
dofile(path .. "/stairs.lua")
dofile(path .. "/torches.lua")
dofile(path .. "/recipes.lua")
dofile(path .. "/player_inventory.lua")
assert(loadfile(path .. "/skeletons.lua"))(path)
