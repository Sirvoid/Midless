# Midless Lua API

Put `.lua` files inside:

```text
mods/
```

The API is available through the global `midless` table.

---

## Positions

Positions use tables:

```lua
local pos = {x = 10, y = 20, z = 30}
```

You can also use the `vector` library:

```lua
local pos = vector.new(10, 20, 30)
```

---

# World

## Get a block

```lua
local block = midless.get_block({x = 10, y = 20, z = 10})
if block:is_loaded() then
    local id = block:get_id() -- "midless:air" is air
    local position = block:get_position()
    block:set_id("midless:stone")
end
```

Returns a block object at the given position. Use `:get_id()` when you need the
identifier. `get_position()` and `is_loaded()` work even when the chunk is
unloaded; other methods require a loaded chunk. The object also exposes
`get_metadata`, `set_metadata`, `reset_metadata`, and `get_inventory`.
See [metadata](#metadata).

## Set a block

```lua
midless.set_block(pos, blockId)
```

Example:

```lua
midless.set_block({x = 10, y = 20, z = 10}, "midless:stone")
```

Use `"midless:air"` (or `0`) to place air.

## Set multiple blocks

```lua
midless.set_blocks(updates, callCallbacks)
```

Example:

```lua
midless.set_blocks({
    {pos = {x = 10, y = 20, z = 10}, blockId = 1},
    {pos = {x = 11, y = 20, z = 10}, blockId = 1},
    {pos = {x = 12, y = 20, z = 10}, blockId = 0},
}, false)
```

`callCallbacks` controls whether `on_block_update` is called. It defaults to `true`.

---

# Messages

## Broadcast

Send a message to every player:

```lua
midless.broadcast("Hello everyone!")
```

## Private message

```lua
player:send_message("Hello!")
```

---

# Events

Events let mods react to things happening in the game.

## Server ready

```lua
midless.register_on_ready(function()
    print("Server ready!")
end)
```

## Server step

Called every simulation update.

```lua
midless.register_on_step(function(dt)
    -- dt = elapsed time in seconds
end)
```

## Player joins

```lua
midless.register_on_player_join(function(player)
    player:send_message("Welcome!")
end)
```

## Player leaves

```lua
midless.register_on_player_leave(function(player)
    print(player:get_name() .. " left")
end)
```

## Player message

```lua
midless.register_on_player_message(function(player, message)
    print(player:get_name() .. ": " .. message)
end)
```

Return `true` to stop the message from appearing in chat:

```lua
midless.register_on_player_message(function(player, message)
    if message:sub(1, 1) == "!" then
        return true
    end

    return false
end)
```

## Player click

```lua
midless.register_on_player_click(function(player, button)
    if button == "left" then
        print("Left click")
    elseif button == "right" then
        print("Right click")
    end
end)
```

## Block update

```lua
midless.register_on_block_update(function(pos, blockId, previousBlockId)
    print("Block changed!")
end)
```

---

# Players

## Get all players

```lua
local players = midless.get_players()
```

Example:

```lua
for _, player in ipairs(midless.get_players()) do
    print(player:get_name())
end
```

## Find player by name

```lua
local player = midless.get_player_by_name("Sirvoid")
```

Returns `nil` if the player was not found.

Names are case-sensitive.

## Find player by ID

```lua
local player = midless.get_player_by_id(id)
```

---

# Player Methods

## ID

```lua
local id = player:get_id()
```

## Name

```lua
local name = player:get_name()
```

## Position

```lua
local pos = player:get_position()
```

## Eye position

```lua
local pos = player:get_eye_position()
```

## Look direction

```lua
local direction = player:get_look_direction()
```

## Teleport

```lua
player:teleport({x = 0, y = 80, z = 0})
```

## Change model

```lua
player:set_model(modelName)
```

## Send message

```lua
player:send_message("Hello!")
```

---

# Entities

Entities are custom objects controlled by Lua.

## Define an entity

```lua
midless.define_entity("my_mod:example", {
    save = true, -- default; false prevents this entity from saving.
    model = "my_mod:cube",

    on_spawn = function(self)
        print("Spawned!")
    end,

    on_step = function(self, dt)
        -- Called every simulation update
    end,

    on_remove = function(self)
        print("Removed!")
    end,
})
```

Store temporary entity data on `self`. Declare `metadata` for values that should save; see [saving entities](#saving-entities).

```lua
on_spawn = function(self)
    self.age = 0
end
```

The entity object is:

```lua
self.object
```

## Spawn an entity

```lua
local entity = midless.spawn_entity(
    "my_mod:example",
    {x = 0, y = 80, z = 0}
)
```

---

# Entity Methods

## ID

```lua
entity:get_id()
```

## Check if valid

```lua
if entity:is_valid() then
    -- entity still exists
end
```

## Position

```lua
local pos = entity:get_position()

entity:set_position({
    x = 10,
    y = 20,
    z = 10
})
```

## Rotation

Rotation uses XYZ Euler angles in radians: `x` is pitch, `y` is yaw, and `z` is roll.

```lua
local rotation = entity:get_rotation()

entity:set_rotation({
    x = 0,
    y = math.pi,
    z = 0
})
```

## Change model

```lua
entity:set_model(modelName)
```

## Remove

```lua
entity:remove()
```

---

# Textures

## Define a texture

Load a PNG texture:

```lua
midless.define_texture(modelName, filePath)
```

The texture file is loaded from the server's working directory.

Use a unique namespaced name for your texture.

## Use a texture on an entity model

Set the model's `texture` to the texture name:

```lua
midless.define_texture("my_mod:slime", "slime.png")

midless.define_entity_model("my_mod:slime", {
    texture = "my_mod:slime",

    parts = {
        ...
    }
})
```

## Replace the terrain texture

The terrain texture must be a `256x256` atlas:

```lua
midless.define_texture("my_mod:terrain", "terrain.png")
midless.set_terrain_texture("my_mod:terrain")
```

Restore the default terrain:

```lua
midless.set_terrain_texture("terrain")
```

## Update a texture

Define the same texture name again:

```lua
midless.define_texture("my_mod:slime", "new_slime.png")
```

Models using that texture update automatically.

## Limits

* PNG only
* Maximum 64 custom textures
* Maximum texture size: `1024x1024`
* Terrain textures must be `256x256`
* `"terrain"` and `"humanoid"` are built-in texture names

---

# Entity Models

Custom entity models can be created from boxes.

```lua
midless.define_entity_model("my_mod:cube", {
    name = "Cube",
    texture = "terrain",

    parts = {
        {
            role = model.part.NONE,
            position = {0, 0, 0},
            min = {-4, 0, -4},
            max = {4, 8, 4},

            uv = {
                east  = {0, 0, 16, 16},
                west  = {0, 0, 16, 16},
                up    = {0, 0, 16, 16},
                down  = {0, 0, 16, 16},
                north = {0, 0, 16, 16},
                south = {0, 0, 16, 16}
            }
        }
    }
})
```

`16` model units = `1` block.

UVs use:

```text
{x, y, width, height}
```

in image pixels.

Available textures are `"humanoid"`, `"terrain"`, and any name registered with `midless.define_texture`. See [Textures](#textures).

## Model part roles

```lua
model.part.NONE
model.part.HEAD
model.part.RIGHT_ARM
model.part.LEFT_ARM
model.part.RIGHT_LEG
model.part.LEFT_LEG
```

Roles allow the normal player animation system to animate those parts.

## Held block position

The first `model.part.RIGHT_ARM` part holds the selected block at the bottom center of its bounds.

Add `grip` to that part to change the position:

```lua
grip = {-1.25, -9, -0.5}
```

Coordinates use model units relative to the part's pivot.

## Remove model

```lua
midless.remove_entity_model(modelName)
```

## Change an entity model

```lua
entity:set_model(modelName)
```

---

# Blocks

## Define a block

Use a unique identifier prefixed with your mod's name:

```lua
midless.define_block("example:stone", {
    name = "Example Block",

    textures = {
        all = 1
    }
})
```

You can assign individual textures:

```lua
textures = {
    sides = 1,
    top = 2,
    bottom = 3
}
```

Available faces:

```text
all
sides
left
right
top
bottom
front
back
```

## Block bounds

Block geometry uses coordinates from `0` to `16`.

A full block:

```lua
bounds = {
    min = {0, 0, 0},
    max = {16, 16, 16}
}
```

A half block:

```lua
bounds = {
    min = {0, 0, 0},
    max = {16, 8, 16}
}
```

Example:

```lua
midless.define_block(19, {
    name = "Half Block",

    textures = {
        all = 1,
        top = 2
    },

    bounds = {
        min = {0, 0, 0},
        max = {16, 8, 16}
    }
})
```

---

# Block Constants

## Model

```lua
block.model.GAS
block.model.SOLID
block.model.SPRITE
```

## Rendering

```lua
block.render.OPAQUE
block.render.TRANSPARENT
block.render.TRANSLUCENT
```

## Collision

```lua
block.collider.NONE
block.collider.SOLID
block.collider.LIQUID
```

## Lighting

```lua
block.light.NONE
block.light.EMIT
```

Example:

```lua
midless.define_block(19, {
    name = "Glass",

    textures = {
        all = 17
    },

    model = block.model.SOLID,
    render = block.render.TRANSPARENT,
    collider = block.collider.SOLID,
    light = block.light.NONE
})
```

# World Generation

World generation is defined in Lua when mods load. The engine handles the actual chunk generation.

```lua
local wg = midless.worldgen
local f = wg.field
```

## Basic setup

Use `wg.configure()` to configure the world:

```lua
wg.configure({
    id = "my_mod:world",
    version = 1,

    min_y = -128,
    max_y = 256,
    sea_level = 48,

    temperature = f.noise2d({frequency = 0.001}),
    moisture = f.noise2d({frequency = 0.001}),
})
```

Without a world generation mod, the world is flat at `y = 64`.

Existing chunks are never regenerated when the generator changes.

---

## Fields

Fields are values calculated from world coordinates.

```lua
local height = f.noise2d({
    frequency = 0.01,
    octaves = 4,
})

local caves = f.noise3d({
    frequency = 0.03,
})
```

They can be combined like normal numbers:

```lua
local terrain = f.noise3d({frequency = 0.02}) - f.y() / 100
```

### Coordinates

```lua
f.x()
f.y()
f.z()
```

### Math

```lua
a + b
a - b
a * b
a / b
a % b

f.min(a, b)
f.max(a, b)
f.abs(a)

f.floor(a)
f.ceil(a)
f.trunc(a)

f.sin(a)
f.cos(a)
```

### Conditions

```lua
f.lt(a, b)
f.eq(a, b)
f.select(condition, yes, no)
```

Conditions return `0` or `1`.

### Noise

```lua
f.noise2d({
    type = "opensimplex2s",
    frequency = 0.01,
    octaves = 3,
})

f.noise3d({
    frequency = 0.02,
})
```

Common noise options:

| Option        | Default           |
| ------------- | ----------------- |
| `type`        | `"opensimplex2s"` |
| `fractal`     | `"fbm"`           |
| `frequency`   | `0.01`            |
| `octaves`     | `3`               |
| `lacunarity`  | `2`               |
| `gain`        | `0.5`             |
| `seed_offset` | `0`               |

Noise types:

```text
opensimplex2
opensimplex2s
cellular
perlin
value_cubic
value
```

Fractal types:

```text
none
fbm
ridged
pingpong
```

---

## Terrain

Terrain can be controlled with `density`.

```lua
wg.configure({
    id = "my_mod:world",

    density =
        f.noise3d({frequency = 0.02})
        - f.y() / 100,

    min_y = -128,
    max_y = 256,
    sea_level = 48,
})
```

Positive density is solid.

Zero or negative density is empty.

You can carve caves with another field:

```lua
wg.configure({
    id = "my_mod:world",

    density = terrain,
    caves = f.noise3d({frequency = 0.04}),
})
```

Positive `caves` values carve terrain.

---

## Biomes

Define biomes with `wg.define_biome()`:

```lua
wg.define_biome("my_mod:highlands", {
    temperature = -0.4,
    moisture = 0.5,

    height = 90,
    height_variation = 45,

    height_noise = f.noise2d({
        frequency = 0.008,
        octaves = 5,
    }),

    top = 3,
    filler = 2,
    filler_depth = 3,
    stone = 1,
    underwater = 6,
})
```

`temperature` and `moisture` decide where the biome appears.

`height` and `height_variation` control its terrain.

The material fields control its blocks:

```text
top             Surface block
filler          Blocks below the surface
filler_depth    Filler thickness
stone           Main underground block
underwater      Underwater surface block
```

---

## Ores

Define ores with `wg.define_ore()`:

```lua
wg.define_ore("my_mod:iron", {
    block = 19,
    replaces = {1},

    min_y = -64,
    max_y = 48,

    distribution = "veins",
    size = 12,
    spacing = 16,
    chance = 0.7,
})
```

You can restrict an ore to a biome:

```lua
biome = "my_mod:highlands"
```

### Distributions

```text
clusters    Round deposits
veins       Random-walking veins
layers      Horizontal layers
noise       Places ore using a noise field
```

Example using noise:

```lua
wg.define_ore("my_mod:iron", {
    block = 19,
    replaces = {1},

    distribution = "noise",

    noise = f.noise3d({
        frequency = 0.05,
    }),

    threshold = 0.6,
})
```

---

## Structures

Structures are made from blocks relative to an origin:

```lua
wg.define_structure("my_mod:ruin", {
    blocks = {
        {x = 0, y = 0, z = 0, block = 1},
        {x = 0, y = 1, z = 0, block = 1},
        {x = 1, y = 0, z = 0, block = 1},
    },

    spacing = 160,
    chance = 0.3,

    min_y = 49,
    max_y = 256,

    rotate = true,
})
```

Useful options:

```text
spacing             Distance between placement areas
chance              Chance to generate
min_y / max_y       Height range
biome               Restrict to a biome
max_slope           Maximum terrain height difference
rotate              Random 90° rotation
air_only            Only replace air
foundation          Foundation block
foundation_depth    Maximum foundation depth
```

Block `0` can be used to carve air.

A one-block structure can also be used for things like flowers or decorations.

---

## Trees

Simple trees can be defined as structures:

```lua
wg.define_structure("my_mod:tree", {
    tree = {
        height = 7,
        radius = 3,
        trunk = 10,
        leaves = 11,
    },

    spacing = 32,
    chance = 0.4,
})
```

For more complicated trees and shapes, use procedural features.

---

## Material rules

Rules replace blocks during generation.

```lua
wg.define_rule({
    match = 3,
    block = 2,
    offset_y = -1,
})
```

You can add a condition:

```lua
wg.define_rule({
    match = 3,
    when = f.lt(f.y(), 50),
    block = 2,
})
```

Rules run before ores and structures.

---

## Procedural features

Features can generate shapes such as branches, pillars, rocks, or custom trees.

```lua
wg.define_feature("my_mod:pillars", {
    when =
        f.eq(f.y(), 65)
        * f.eq(f.x() % 16, 0)
        * f.eq(f.z() % 16, 0),

    commands = {
        {
            op = "stroke",
            block = 1,
            steps = 8,

            dx = 0,
            dy = 1,
            dz = 0,

            radius = 1.5,
            bounds = 2,
        },

        {
            op = "sphere",
            block = 4,

            radius = 2.5,
            bounds = 3,
        },
    },
})
```

There are two shape commands:

### `stroke`

Moves in a direction while drawing spheres.

```lua
{
    op = "stroke",

    block = 1,
    steps = 8,

    dx = 0,
    dy = 1,
    dz = 0,

    radius = 1,
    bounds = 2,
}
```

Useful fields inside strokes:

```lua
f.step()
f.steps()
```

These can be used to change the radius along the stroke.

### `sphere`

Draws a sphere:

```lua
{
    op = "sphere",

    block = 1,
    radius = 3,
    bounds = 4,
}
```

Features can chain commands using position slots:

```lua
{
    op = "stroke",
    from = 0,
    to = 1,
    ...
},

{
    op = "sphere",
    from = 1,
    ...
}
```

Slot `0` is the feature's starting position.

Feature origin coordinates are also available:

```lua
f.origin_x()
f.origin_y()
f.origin_z()
```

---

## Example terrain mod

```lua
local wg = midless.worldgen
local f = wg.field

local terrain_noise = f.noise2d({
    frequency = 0.005,
    octaves = 4,
})

wg.configure({
    id = "example:world",
    version = 1,

    min_y = -128,
    max_y = 256,
    sea_level = 48,

    temperature = f.noise2d({
        frequency = 0.001,
        seed_offset = 10,
    }),

    moisture = f.noise2d({
        frequency = 0.001,
        seed_offset = 20,
    }),
})

wg.define_biome("example:plains", {
    temperature = 0,
    moisture = 0,

    height = 64,
    height_variation = 12,
    height_noise = terrain_noise,

    top = 3,
    filler = 2,
    filler_depth = 3,
    stone = 1,
})

wg.define_ore("example:iron", {
    block = 19,
    replaces = {1},

    min_y = -64,
    max_y = 32,

    distribution = "veins",
    size = 8,
    spacing = 24,
    chance = 0.5,
})

wg.define_structure("example:tree", {
    tree = {
        height = 6,
        radius = 3,
        trunk = 10,
        leaves = 11,
    },

    spacing = 32,
    chance = 0.3,

    biome = "example:plains",
})
```

---

# Vectors

Create a vector:

```lua
local v = vector.new(1, 2, 3)
```

Zero vector:

```lua
local v = vector.new()
```

Copy:

```lua
local copy = vector.new(v)
```

## Add

```lua
vector.add(a, b)
```

## Subtract

```lua
vector.subtract(a, b)
```

## Multiply

```lua
vector.multiply(v, 5)
```

## Length

```lua
vector.length(v)
```

## Distance

```lua
vector.distance(a, b)
```

## Normalize

```lua
vector.normalize(v)
```

## Direction

```lua
vector.direction(from, to)
```

## Dot product

```lua
vector.dot(a, b)
```

## Cross product

```lua
vector.cross(a, b)
```

Example:

```lua
local spawnPos = vector.add(
    player:get_eye_position(),
    vector.multiply(player:get_look_direction(), 3)
)
```

This gets a position three blocks in front of the player.

---

# Timers

`midless.sleep()` exists:

```lua
midless.sleep(1000)
```

However, it blocks the server.

For gameplay timers, use `register_on_step` instead:

```lua
local timer = 0

midless.register_on_step(function(dt)
    timer = timer + dt

    if timer >= 5 then
        timer = 0
        print("Five seconds!")
    end
end)
```

---

# Example Mod

This example creates a simple slime that wanders around.

```lua
midless.define_texture("example:slime", "slime.png")

midless.define_entity_model("example:slime", {
    texture = "example:slime",

    parts = {
        {
            role = model.part.NONE,
            position = {0, 0, 0},
            min = {-5, 0, -5},
            max = {5, 8, 5},

            uv = {
                east  = {0, 0, 16, 16},
                west  = {0, 0, 16, 16},
                up    = {0, 0, 16, 16},
                down  = {0, 0, 16, 16},
                north = {0, 0, 16, 16},
                south = {0, 0, 16, 16}
            }
        }
    }
})

midless.define_entity("example:slime", {
    model = "example:slime",

    on_spawn = function(self)
        self.direction = vector.new(1, 0, 0)
        self.timer = 0
    end,

    on_load = function(self)
        self.direction = vector.new(1, 0, 0)
        self.timer = 0
    end,

    on_step = function(self, dt)
        self.timer = self.timer + dt

        -- Pick a new direction every 2 seconds.
        if self.timer >= 2 then
            self.timer = 0

            local angle = math.random() * math.pi * 2

            self.direction = {
                x = math.cos(angle),
                y = 0,
                z = math.sin(angle)
            }

            self.object:set_rotation({
                x = 0,
                y = -angle,
                z = 0
            })
        end

        -- Move the slime.
        local pos = self.object:get_position()

        pos = vector.add(
            pos,
            vector.multiply(self.direction, dt)
        )

        self.object:set_position(pos)
    end
})

-- Spawn a slime three blocks in front of the player.
midless.register_on_player_click(function(player, button)
    if button ~= "right" then
        return
    end

    local pos = vector.add(
        player:get_eye_position(),
        vector.multiply(player:get_look_direction(), 3)
    )

    midless.spawn_entity("example:slime", pos)
end)
```

Right-click to spawn a slime. Each slime picks a new direction every few seconds and moves around on its own.

---

# Metadata

Declare metadata on a block or entity to save custom values, including block
states and inventories.

```lua
midless.define_block(25, {
    name = "Chest",
    textures = {all = 1},
    metadata = {
        {name = "facing", type = "uint", bits = 2, default = 0},
        {name = "open", type = "bool", default = false},
        {name = "items", type = "inventory", slots = 27},
        {name = "label", type = "string", max_length = 64},
    },
})
```

Place that block before accessing its metadata:

```lua
local block = midless.get_block({x = 10, y = 80, z = 20})
block:set_metadata("facing", 2)
print(block:get_metadata("facing"))
block:reset_metadata("facing") -- restore the default
```

Entities use the same methods through `self.object`.

| Type | Options | Default |
| --- | --- | --- |
| `uint` | `bits = 1..32` (default 16) | 0 |
| `int` | `bits = 1..32` (default 16), signed | 0 |
| `bool` | true or false | false |
| `float` | finite number | 0 |
| `string` | `max_length = 0..4096` (default 256) | empty string |
| `inventory` | `slots = 1..255` (default 27) | empty inventory |

Use `default` to change a field's default value. Inventory defaults are always
empty. Tables returned by `get_metadata` are copies;
use `set_metadata` or inventory methods to change the stored data.

Metadata saves automatically with the chunk. Replacing a block with a different
ID clears its metadata.

## Saving entities

```lua
local function initialize(self)
    self.timer = 0 -- temporary Lua data
end

midless.define_entity("example:creature", {
    model = "humanoid",
    save = true, -- default; false makes the entity temporary
    metadata = {
        {name = "health", type = "uint", bits = 7, default = 100},
    },
    on_spawn = initialize,
    on_load = initialize,
})
```

Declared metadata, position, rotation, velocity, body settings, model, and held
block are saved. Ordinary fields on `self` are not saved. Use `on_load` to rebuild
them after loading; saved metadata is already available then. `on_spawn` only runs
for new entities, and `on_remove` does not run when a chunk unloads.

Entities receive a new runtime ID when loaded; old handles become invalid.
`save = false` entities disappear on unload, including any older saved copies.

---

# Items

Blocks automatically have a placeable item with the same identifier. Use
`define_item` for an item that does not place a block:

```lua
midless.define_texture("example:gem", "textures/gem.png")

midless.define_item("example:gem", {
    name = "Gem",
    texture = "example:gem",
    max_stack = 16, -- 1..64; default 64
    metadata = {
        {name = "quality", type = "uint", bits = 4, default = 1},
        {name = "label", type = "string", max_length = 32},
    },
})
```

Set `texture` to a name registered with `define_texture`. Multiple items can use
the same texture. Use a PNG up to 64 by 64 pixels with a transparent background. It becomes the
inventory icon and a sprite with one pixel of thickness when held or dropped.
`held_model = "sprite"` is optional; it is the default for ordinary items.

## Item metadata

Set metadata when creating a stack:

```lua
local inventory = player:get_inventory()
inventory:set_stack(1, {
    id = "example:gem", count = 3,
    metadata = {quality = 2, label = "Found in a cave"},
})

local stack = inventory:get_stack(1)
stack.metadata.quality = 4
inventory:set_stack(1, stack)
```

`get_stack` returns a copy, or `nil` for an empty slot. Omitted metadata uses its
declared defaults. Items only stack together when their IDs and metadata match.
Metadata stays with items when moved, dropped, or saved.

Recipe outputs can include metadata. To match a specific ingredient's metadata,
use a table instead of its identifier:

```lua
midless.define_recipe({
    ingredients = {
        {id = "example:gem", metadata = {quality = 2}},
        "midless:stone",
    },
    output = {id = "example:gem", count = 1, metadata = {quality = 3}},
})
```

# Inventories

```lua
local inventory = block:get_inventory("items")
-- Also available on entities and through player:get_inventory().

inventory:set_stack(1, {id = 1, count = 32})
local stack = inventory:get_stack(1) -- a copy, or nil if empty
inventory:set_stack(1, nil) -- empty the slot

if not inventory:add_item({id = 1, count = 100}) then
    player:send_message("Not enough room.")
end
```

Slots start at 1. Player slots 1-27 are storage and 28-36 are the hotbar.
`add_item` fills matching stacks, then empty slots, splitting large amounts as
needed. The main player inventory prefers empty hotbar slots. It returns `true` if the
whole amount fits, or `false` without changing anything.

## Player inventory screen

Define named inventories for each player, then return the layout to show when
they press E. Put this in a mod file such as `mods/player_inventory.lua`:

```lua
midless.define_player_inventory("crafting", {slots = 4})

midless.define_player_inventory_screen(function(player)
    local crafting = player:get_inventory("crafting")
    return {
        title = "Inventory", width = 9, height = 8,
        elements = {
            {type = "inventory", inventory = crafting,
             x = 0, y = 0, columns = 2, rows = 2},
            {type = "crafting_output", inventory = crafting, recipes = "crafting",
             x = 4, y = 0.5, columns = 2, rows = 2},
            {type = "inventory", inventory = player:get_inventory(),
             x = 0, y = 4, columns = 9, rows = 4},
        },
    }
end)
```

The callback returns the layout each time the player presses E. Register one screen for your mods.

`player:get_inventory()` still returns the main inventory. Named inventories use
the same `get_stack`, `set_stack`, and `add_item` methods and start empty.

## Inventory screens

Define an inventory field in block metadata, then show it together with the
player's inventory:

```lua
midless.define_block(200, {
    name = "Chest",
    textures = {all = 4},
    metadata = {{name = "items", type = "inventory", slots = 27}},

    on_interact = function(player, block)
        player:show_inventory({
            title = "Chest",
            block = block,
            width = 9, height = 8,
            elements = {
                {type = "inventory", inventory = block:get_inventory("items"),
                 x = 0, y = 0, columns = 9, rows = 3},
                {type = "label", text = "Inventory", x = 0, y = 3.5},
                {type = "inventory", inventory = player:get_inventory(),
                 x = 0, y = 4, columns = 9, rows = 4},
            },
        })
    end,
})
```

`on_interact(player, block)` runs when the player right-clicks the block.

Layouts use slot-sized units and scale with the window:

- Set `width` and `height` for the screen size, and `x` and `y` for each element.
- Use `label` for text, `inventory` for slots, `crafting_output` for a result, and `progress` for a bar.
- Include the main player inventory and each additional inventory.
- Use `slot` to choose a grid's first slot (default 1), then `columns` and `rows` for its size.
- Display every inventory slot once. Keep grids inside the screen without overlapping.
- Set `block` when displaying a block inventory. A screen can show one block inventory.

`player:close_inventory()` returns `true` if closed, or `false` if blocked.

## Crafting recipes

```lua
midless.define_recipe({
    group = "crafting", -- default when omitted
    pattern = {
        {4, 4},
        {4, 4},
    },
    output = {id = 202, count = 1},
})
```

Patterns support up to 3 rows and 3 columns. Rows must have the same width;
use `0` for empty cells. The pattern can appear anywhere in the input grid,
but is not automatically rotated or mirrored. Other cells must be empty.
Each occupied recipe cell consumes one item from its slot.

For a shapeless recipe, use `ingredients` instead of `pattern`:

```lua
midless.define_recipe({
    group = "crafting",
    ingredients = {10}, -- one log, in any input slot
    output = {id = 4, count = 4},
})
```

Shapeless recipes support up to 9 entries. Each entry needs a separate occupied
slot, including repeated IDs. Stack sizes determine how many times you can craft.
The result must fit in one stack. If several recipes match, the first registered
recipe in the selected group is used.

## Crafting output

Store the ingredients in block metadata:

```lua
metadata = {
    {name = "ingredients", type = "inventory", slots = 9},
}
```

Display that inventory as a 3-by-3 grid, then add this element to the same screen:

```lua
{
    type = "crafting_output",
    recipes = "crafting",
    inventory = block:get_inventory("ingredients"),
    columns = 3, rows = 3,
    x = 5, y = 1,
}
```

`recipes` selects the recipe group. Other mods can add recipes to the same group.
`columns` and `rows` describe the input grid, which must match a displayed block
or named player inventory and be no larger than 3-by-3. The output itself occupies
one UI slot.

## Rules

Use one inventory for a furnace, with a rule for each slot:

```lua
metadata = {
    {name = "items", type = "inventory", slots = 3, rules = {
        [1] = {items = {6}},       -- input: sand
        [2] = {items = {4, 10}},   -- fuel: planks or logs
        [3] = {insert = false},   -- output: players can only take items
    }},
}
```

Rules apply to player clicks and shift-clicks. Slots without a rule accept any
item.

Position the slots separately in the screen's `elements`:

```lua
local items = block:get_inventory("items")
local elements = {
    {type = "inventory", inventory = items, slot = 1,
     x = 2, y = 0, columns = 1, rows = 1},
    {type = "inventory", inventory = items, slot = 2,
     x = 2, y = 2, columns = 1, rows = 1},
    {type = "inventory", inventory = items, slot = 3,
     x = 6, y = 1, columns = 1, rows = 1},
    {type = "inventory", inventory = player:get_inventory(),
     x = 0, y = 4, columns = 9, rows = 4},
}
```

## Block timers

Add these callbacks to the block definition:

```lua
on_inventory_changed = function(block, field)
    if field == "items" and not block:timer_started() then
        block:start_timer(1) -- seconds
    end
end,

on_timer = function(block, dt)
    -- Use dt (seconds) to burn fuel and advance cooking.
    return true -- repeat; return false or nil to stop
end,
```

`block:start_timer(seconds)` starts or restarts the timer. Use
`block:timer_started()` to check it and `block:stop_timer()` to stop it.
Timers pause while the chunk is unloaded and resume when it loads.

`on_inventory_changed(block, field)` runs after inventory changes, including Lua
changes. Several changes may share one call. Changes inside this callback do not
trigger another call.

## Inventory transactions

Use `block:inventory_transaction(name, changes)` to consume ingredients and add
their result together:

```lua
local cooked = block:inventory_transaction("items", {
    take = {{slot = 1, id = 6, count = 1}},
    give = {{slot = 3, id = 14, count = 1}},
})
```

It returns `true` on success, or `false` without changing anything if the items
are missing or the result does not fit. Each entry specifies a slot, item ID,
and count. Omit `take` or `give` when you only need the other operation.

## Progress bars

Declare a numeric metadata field such as `cook_progress`, then add this element
to a block screen:

```lua
{type = "progress", value = "cook_progress", max = 10,
 x = 3.5, y = 1.25, width = 2, height = 0.5},
```

The bar updates automatically when that metadata changes. `max` is the value
for a full bar. You can also use a number for a fixed `value`.
