local wg = midless.worldgen
local f = wg.field
local x, y, z = f.x(), f.y(), f.z()
local ox, oy, oz = f.origin_x(), f.origin_y(), f.origin_z()

local function noise2(px, pz, octaves, gain, lacunarity)
    return f.noise2d({
        type = "opensimplex2s", fractal = "fbm", frequency = 0.01,
        octaves = octaves, gain = gain, lacunarity = lacunarity,
        x = px, z = pz,
    })
end

local function noise3(px, py, pz, octaves, gain, lacunarity, fractal)
    return f.noise3d({
        type = "opensimplex2s", fractal = fractal or "fbm", frequency = 0.01,
        octaves = octaves, gain = gain, lacunarity = lacunarity,
        x = px, y = py, z = pz,
    })
end

local elevation = noise2(x * 0.05, z * 0.05, 1, 0.75, 1) + 1

local function terrain_state(py)
    local offset = noise3(x * 2, py * 2, z * 2, 2, 1, 2)
        * 12 * (elevation - 1)
    local terrain = (noise3((x + offset) * 2.5, offset,
        (z + offset) * 2.5, 3, 0.75, 1) * offset + 1) * 32 * elevation
    local a = noise3(x * 1.5, py * 1.5, z * 1.5, 1, 1, 2, "ridged")
    local b = noise3(x * 1.5, (py + 2048) * 1.5, z * 1.5, 1, 1, 2, "ridged")
    local interior = f.select(f.lt(0.6, a * b), 2, 1)
    return f.select(f.lt(py, 16), interior,
        f.select(f.lt(py + terrain, 96 * elevation), interior, 0))
end

local state, above = terrain_state(y), terrain_state(y + 1)
local surface = f.eq(state, 1) * f.eq(above, 0)
local solid = f.eq(state, 1)
local material = f.select(f.eq(state, 2), 0,
    f.select(surface, f.select(f.lt(48, y), 3, 6),
        f.select(solid, 1, f.select(f.lt(y, 48), 5, 0))))

wg.configure({
    id = "midless:classic", version = 2,
    min_y = -128, max_y = 896, bounded = false,
    material = material,
    density = solid,
    skylight = solid,
    ceiling = f.ceil(64 * elevation
        + 384 * elevation * f.abs(elevation - 1)) + 1,
})

local function ore_vein(name, block, max_y, size, spacing, chance)
    wg.define_ore("base_game:" .. name, {
        block = block, replaces = {1},
        min_y = -128, max_y = max_y,
        distribution = "veins",
        size = size, spacing = spacing, chance = chance,
    })
end

ore_vein("coal", 8, 128, 16, 12, 0.8)
ore_vein("iron", 7, 64, 12, 16, 0.7)
ore_vein("gold", 9, 16, 8, 20, 0.4)

local function random_at(px, py, pz, salt)
    return f.random({
        seed = f.trunc(px * 1135 + py * 1307 + pz * 1479) % 2048,
        salt = salt, seed_scale = 1024, salt_scale = 1024,
    })
end
local function random_here(salt) return random_at(x, y, z, salt) end
local function random_origin(salt) return random_at(ox, oy, oz, salt) end

wg.define_rule({
    match = 3, when = f.lt(256, f.local_index()), offset_y = -1, block = 2,
})
local flower = random_here(6) % 128
wg.define_rule({match = 3, when = f.eq(flower, 0), offset_y = 1, block = 12, descending = true})
wg.define_rule({match = 3, when = f.eq(flower, 1), offset_y = 1, block = 13, descending = true})

local step, steps = f.step(), f.steps()
local function stroke(from, to, thickness, upwardness, length, salt, when)
    local angle = (random_here(10 + salt) % 360) * (math.pi / 180)
    return {
        op = "stroke", from = from, to = to, when = when, block = 10,
        steps = length * 4,
        dx = f.cos(angle) / upwardness, dy = 0.25,
        dz = f.sin(angle) / upwardness,
        bounds = thickness,
        radius = thickness / (2 + step / (steps / 1.5)),
    }
end

local commands = {
    stroke(0, 1, 5, 16, 8 + random_origin(1) % 3, 10),
}
local count = 4 + random_origin(2) % 2
for i = 0, 4 do
    local enabled = f.lt(i, count)
    commands[#commands + 1] = stroke(1, 2, 3, 4,
        5 + random_origin(3 * i) % 2, i, enabled)
    local thickness = 9 + random_origin(4 * i) % 3
    commands[#commands + 1] = {
        op = "sphere", from = 2, to = 2, when = enabled, block = 11,
        bounds = thickness, radius = thickness / 2,
    }
end

local allowed_column = (1 - f.eq(f.floor(x / 16), 0))
    * (1 - f.eq(f.floor(z / 16), 0))
wg.define_feature("midless:branching_tree", {
    when = f.select(allowed_column,
        f.select(f.lt(y, 48), 0,
            f.select(surface, f.eq(random_here(5) % 512, 0), 0)), 0),
    origin_min = {-1, -2, -1}, origin_max = {1, 1, 1},
    commands = commands,
})
