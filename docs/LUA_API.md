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
local id = midless.get_block(pos)
```

Example:

```lua
local block = midless.get_block({x = 10, y = 20, z = 10})
```

Returns the block ID.

`0` represents air. Unloaded chunks also return `0`.

## Set a block

```lua
midless.set_block(pos, blockId)
```

Example:

```lua
midless.set_block({x = 10, y = 20, z = 10}, 1)
```

Use block ID `0` to place air.

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

Store entity-specific data on `self`:

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

Custom block IDs can use IDs `19` through `255`.

```lua
midless.define_block(19, {
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
