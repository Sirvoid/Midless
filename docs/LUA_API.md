# Midless Lua API

- [Getting started](#getting-started)
- [Events and timers](#events-and-timers)
- [Players](#players)
- [World and blocks](#world-and-blocks)
- [Items and digging](#items-and-digging)
- [Metadata](#metadata)
- [Inventories and crafting](#inventories-and-crafting)
- [Entities](#entities)
- [Mobs and spawning](#mobs-and-spawning)
- [Textures and models](#textures-and-models)
- [World generation](#world-generation)
- [Vectors](#vectors)

## Getting started

Put a `.lua` file or a mod folder in `mods/`. A folder needs an `init.lua`, which receives its path as `...`:

```lua
-- mods/my_mod/init.lua
local path = ...
dofile(path .. "/items.lua")
dofile(path .. "/blocks.lua")
midless.define_texture("my_mod:gem", path .. "/textures/gem.png")
```

Only `init.lua` loads automatically inside a mod folder. Top-level files and folders load alphabetically; load your other scripts in the order they need. Use `midless.register_on_ready` for setup that depends on other mods.

The API uses the global `midless` table. Use namespaced identifiers such as `"my_mod:stone"`. File paths are relative to the server's working directory unless absolute.

Positions and directions use `{x = 10, y = 20, z = 30}` or `vector.new(10, 20, 30)`.

## Events and timers

Register a callback with `midless.register_on_...(function(...) ... end)`:

| Registration | Callback arguments | Behavior |
| --- | --- | --- |
| `register_on_ready` | none | Server ready; useful for mod dependencies. |
| `register_on_step` | `dt` | Every simulation update; `dt` is seconds. |
| `register_on_player_join` | `player` | Player joins. |
| `register_on_player_leave` | `player` | Player leaves. |
| `register_on_player_message` | `player, message` | Return `true` to hide the message from chat. |
| `register_on_player_land` | `player, distance` | Player lands; use distance for custom fall damage. |
| `register_on_player_click` | `player, button` | Button is `"left"` or `"right"`. |
| `register_on_player_attack` | `player, target` | Targeted attack; return `true` to stop later handlers. |
| `register_on_player_damage` | `player, amount, context` | Before damage; see [Damage](#damage). |
| `register_on_hp_change` | `player, old_hp, new_hp` | After HP changes. |
| `register_on_block_update` | `pos, blockId, previousBlockId` | Block changes. |
| `register_on_dig_time` | `player, block, stack, seconds` | Adjust or cancel digging; see [Items and digging](#items-and-digging). |

Metadata listeners also take the field name:

```lua
midless.register_on_player_metadata_change("my_mod:air", function(player, old, new)
    print(new)
end)
```

Use a step callback for timers. `midless.sleep(milliseconds)` blocks the server.

```lua
local elapsed = 0
midless.register_on_step(function(dt)
    elapsed = elapsed + dt
    if elapsed >= 5 then
        elapsed = elapsed - 5
        midless.broadcast("Five seconds!")
    end
end)
```

## Players

### Lookup and methods

| Call | Result or action |
| --- | --- |
| `midless.get_players()` | All players. |
| `midless.get_player_by_name(name)` | Case-sensitive lookup; `nil` if not found. |
| `midless.get_player_by_id(id)` | Look up by runtime ID. |
| `player:is_valid()` | Connected and ready for movement. Check retained handles before use. |
| `player:get_id()` / `player:get_name()` | Runtime ID / username. |
| `player:get_position()` | Position. |
| `player:get_eye_position()` | Eye position. |
| `player:get_look_direction()` | Look direction. |
| `player:teleport(pos)` | Move instantly. |
| `player:apply_impulse(velocity)` | Add velocity in blocks/second. |
| `player:get_spawn_point()` / `player:set_spawn_point(pos)` | Persistent spawn point. |
| `player:get_hp()` / `player:set_hp(hp)` | Persistent HP. |
| `player:damage(amount, context)` | Apply damage; returns HP lost. |
| `player:send_message(text)` | Private chat message. |
| `player:set_model(name)` | Change model. |
| `player:get_texture()` / `player:set_texture(name)` | Texture override; `nil` restores the model default. |
| `player:set_nametag(options)` | Set text, color, visibility, or offset. |
| `player:get_inventory(name)` | Main inventory when name is omitted. |
| `player:get_selected_slot()` | Selected slot in the main inventory, starting at 1. |
| `player:get_selected_stack()` | Selected stack copy, or `nil`. |
| `player:set_selected_stack(stack)` | Replace selected stack; `nil` clears it. |
| `player:get_metadata(field)` | Read a declared field. |
| `player:set_metadata(field, value)` / `player:reset_metadata(field)` | Change a field / restore its default. |
| `player:set_hud_bar(name, options)` | Update a HUD bar's value or visibility. |
| `player:show_inventory(layout)` | Open an inventory screen. |
| `player:close_inventory()` | `true` if closed, `false` if blocked. |

The default spawn is just above the highest solid or liquid surface at X/Z `(0, 0)`.

Players start with 20 HP. HP must be an integer from 0 to 65535. **Zero HP does not automatically kill or respawn a player**; handle that in your mod.

### Chat and nametags

```lua
midless.broadcast("Hello everyone!")
player:send_message("&cRed &fWhite")
player:set_nametag({text = "&c[Admin] &fAlex"})
entity:set_nametag({text = "Merchant", color = "#FFFFFF", visible = true, offset = 0.5})
```

Chat and nametags share color codes: `&0`–`&9` and `&a`–`&f` (uppercase also works). Use `&&` for a literal ampersand, or `midless.escape_text(text)` to escape user text.

```lua
midless.define_text_color("g", "#FFD166")
midless.remove_text_color("g")
```

Custom codes are case-sensitive, single printable ASCII characters except `&`, `%`, or space. Colors accept `#RRGGBB` or `#RRGGBBAA`; alpha `00` removes a definition. Overriding a standard code is allowed; removal restores its default.

Players default to their username; entities default to an empty nametag. Entity tags save and are restored before `on_load`. Set `{visible = false}` to hide a tag.

### HUD bars

Use a registered **9×9 PNG**:

```lua
midless.define_texture("my_mod:heart", "textures/heart.png")
midless.define_hud_bar("my_mod:health", {
    texture = "my_mod:heart", max = 20,
    icons = 10,   -- Default 10; range 1–16.
    priority = 0, -- Lower numbers first; default 0.
})
midless.register_on_player_join(function(player)
    player:set_hud_bar("my_mod:health", {value = player:get_hp(), visible = true})
end)
midless.register_on_hp_change(function(player, old, new)
    player:set_hud_bar("my_mod:health", {value = new})
end)
```

Bars pack two per row above the hotbar, left to right and then upward. Hidden bars leave no gap. Equal priorities use registration order; removed IDs may be reused. `midless.remove_hud_bar(name)` removes a bar for everyone.

## World and blocks

### Read and change blocks

```lua
local pos = {x = 10, y = 20, z = 10}
local b = midless.get_block(pos)
if b:is_loaded() then
    print(b:get_id())
    b:set_id("midless:stone")
end
midless.set_block(pos, "midless:air")
midless.set_blocks({
    {pos = {x = 10, y = 20, z = 10}, blockId = "midless:stone"},
    {pos = {x = 11, y = 20, z = 10}, blockId = 0},
}, false)
```

Air is `"midless:air"` or `0`. The second `set_blocks` argument controls block-update callbacks and defaults to `true`.

Block objects expose `get_position`, `is_loaded`, `get_id`, `set_id`, metadata methods, inventory methods, and timers. Only `get_position()` and `is_loaded()` work while the chunk is unloaded.

### Define blocks

```lua
midless.define_block("my_mod:stone", {
    name = "Stone",
    textures = {all = 1},
    hardness = 3,
    dig_group = "stone",
})
```

`textures` accepts terrain atlas tiles for `all`, `sides`, `left`, `right`, `top`, `bottom`, `front`, and `back`.

| Field | Constants |
| --- | --- |
| `model` | `block.model.GAS`, `SOLID` (default), `SPRITE` |
| `render` | `block.render.OPAQUE`, `TRANSPARENT`, `TRANSLUCENT` |
| `collider` | `block.collider.NONE`, `SOLID`, `LIQUID` |
| `light` | `block.light.NONE`, `EMIT` |

Use the full prefix for each constant, such as `block.render.TRANSPARENT`.

### Shapes and states

Geometry uses integer coordinates from 0 to 16. For a single shape, use `bounds`:

```lua
bounds = {min = {0, 0, 0}, max = {16, 8, 16}}, -- Half block.
```

For multiple shapes, use 1–8 `boxes`, each with `min < max` and optional `textures`:

```lua
boxes = {
    {min = {0, 0, 0}, max = {16, 8, 16}},
    {min = {0, 8, 8}, max = {16, 16, 16}},
},
```

For state-dependent shapes, declare metadata, named `models`, and `variants`:

```lua
midless.define_block("my_mod:door", {
    name = "Door", textures = {all = 6},
    metadata = {
        {name = "facing", type = "uint", bits = 2, default = 0},
        {name = "open", type = "bool", default = false},
    },
    state_fields = {"facing", "open"},
    models = {
        closed = {boxes = {{min = {0, 0, 0}, max = {16, 16, 3}}}},
        open = {boxes = {{min = {0, 0, 0}, max = {3, 16, 16}}}},
    },
    variants = {
        {when = {open = false}, model = "closed", rotate_y_from = "facing"},
        {when = {open = true}, model = "open", rotate_y_from = "facing"},
    },
    on_interact = function(player, door)
        door:set_metadata("open", not door:get_metadata("open"))
    end,
})
```

`state_fields` supports up to 8 fields. The first matching variant wins; `when = {}` matches anything. Variants may override `boxes`, `textures`, `render`, `light`, and `collider`.

`rotate_y` accepts 0/90/180/270 degrees. `rotate_y_from` reads metadata values 0–3; other values use 0. Both rotations combine.

Collision and selection default to the model boxes. Override with `collision_boxes` or `selection_boxes` (0–8 boxes each). Empty selection boxes make a block untargetable.

### Block callbacks and timers

| Definition callback | When / return value |
| --- | --- |
| `on_place(player, placed)` | After player placement; not called by `set_block`. |
| `on_interact(player, block)` | Player right-clicks the block. |
| `on_use(player, stack, target)` | Uses the block's item; see [Item callbacks](#item-callbacks). |
| `on_dig(player, block, stack)` | Its item successfully breaks a block. |
| `on_inventory_changed(block, field)` | After inventory changes, including Lua changes. |
| `on_timer(block, dt)` | Timer fires; return `true` to repeat, `false`/`nil` to stop. |

Use this placement callback with the door's `facing` field to face its +Z side toward the player:

```lua
on_place = function(player, placed)
    local look = player:get_look_direction()
    local facing = math.abs(look.x) > math.abs(look.z)
        and (look.x > 0 and 1 or 3) or (look.z > 0 and 2 or 0)
    placed:set_metadata("facing", facing)
end,
```

`block:start_timer(seconds)` starts or restarts a timer; `timer_started()` checks it and `stop_timer()` stops it. Timers pause on chunk unload and resume on load. `dt` is seconds.

Inventory changes may be combined into one callback. Changes inside `on_inventory_changed` do not trigger another call.

## Items and digging

Blocks automatically get placeable items with the same identifier. Define other items with `define_item`:

```lua
midless.define_texture("my_mod:pickaxe", "textures/pickaxe.png")
midless.define_item("my_mod:pickaxe", {
    name = "Pickaxe", texture = "my_mod:pickaxe", max_stack = 1,
    dig_speed = {stone = 6},
    harvest_levels = {stone = 2},
    metadata = {{name = "durability", type = "uint", bits = 8, default = 100}},
    on_dig = function(player, block, stack)
        stack.metadata.durability = stack.metadata.durability - 1
        player:set_selected_stack(stack.metadata.durability > 0 and stack or nil)
    end,
})
```

`max_stack` is 1–64, default 64. Item textures are registered PNGs up to 64×64 pixels, normally with transparency. They provide the inventory icon and a one-pixel-thick held/dropped sprite. `held_model = "sprite"` is the default.

### Stacks

```lua
local stack = {id = "my_mod:pickaxe", count = 1, metadata = {durability = 50}}
player:set_selected_stack(stack)
```

Stack getters return copies or `nil` when empty. Write a changed stack back with `set_stack` or `set_selected_stack`; use `nil` to clear it. Omitted metadata uses defaults. Items stack only when ID and metadata match, and metadata survives moving, dropping, and saving.

### Breaking and harvest levels

| Block field | Meaning |
| --- | --- |
| `hardness` | Base break time: 0–86400 seconds, default 1. Zero is instant. |
| `dig_group` | Tool speed and harvest group, such as `"stone"`. |
| `unbreakable` | `true` prevents digging. |
| `harvest_level` | Required tool level for loot: 0–255, default 0. |
| `drops` | Loot list or callback; omitted means one of the block's own items. |

Break time is **hardness / tool speed**. Missing speed groups and empty hands use 1; speeds must be positive. Missing harvest groups and empty hands have level 0. An under-level tool still breaks the block but gets no loot.

| Built-in blocks | Group | Hardness |
| --- | --- | --- |
| Stone / stone slab | `stone` | 3 / 2 |
| Iron, coal, gold ore | `stone` | 4 |
| Wood, log, wood slab / leaves | `wood` | 2 / 0.2 |
| Dirt, sand / grass | `soil` | 0.5 / 0.6 |
| Glass | `glass` | 0.3 |
| Rose, dandelion | `plant` | 0 |
| Air | `gas` | 0 |
| Water, lava | `liquid` | 0 |
| Fire | `fire` | 0 |

`register_on_dig_time` callbacks run in registration order when digging starts. Return seconds to change the duration, `nil` to keep it, or `false` to cancel. They cannot override `unbreakable`.

### Drops

In a block definition, use `drops = {}` for no loot, a list, or a function:

```lua
drops = {"midless:stone", {id = "my_mod:gem", count = 2, metadata = {quality = 3}}},
-- Alternatively:
drops = function(player, block, stack)
    -- stack is the tool, or nil. The original block metadata is still available.
    return math.random(10) == 1 and {"my_mod:gem"} or {}
end,
```

The function runs after digging finishes and harvest requirements pass. Return a list; each entry must fit its item's stack limit. Split larger amounts across entries.

### Item callbacks

`on_use(player, stack, target)` handles right-click use on items and block items:

| `target.type` | Extra fields |
| --- | --- |
| `"block"` | `target.block`, `target.normal` (clicked face vector) |
| `"entity"` | `target.entity` |
| `"nothing"` | None |

Return `true` to consume the action, or `false`/`nil` to allow normal placement.

`on_dig(player, block, stack)` runs after a successful break, even without loot. `block` now refers to the empty position; `stack` is a copy of the tool used. Write it back to apply durability changes.

### Breaking texture

```lua
midless.define_texture("my_mod:breaking", "textures/breaking.png")
midless.set_breaking_texture("my_mod:breaking")
```

Use ten square frames in a horizontal PNG strip, least to most cracked: for example, 160×16. Frames may be up to 64 pixels wide. A default texture is provided; redefining your texture updates the animation.

## Metadata

Declare persistent fields in a block, item, or entity definition's `metadata` list:

```lua
metadata = {
    {name = "facing", type = "uint", bits = 2, default = 0},
    {name = "open", type = "bool", default = false},
    {name = "label", type = "string", max_length = 64},
    {name = "items", type = "inventory", slots = 27},
},
```

| Type | Options | Default |
| --- | --- | --- |
| `uint` | `bits = 1..32`, default 16 | 0 |
| `int` | Signed; `bits = 1..32`, default 16 | 0 |
| `bool` | Boolean | false |
| `float` | Finite number | 0 |
| `string` | `max_length = 0..4096`, default 256 | Empty string |
| `inventory` | `slots = 1..255`, default 27 | Empty inventory |

Use `default` to change defaults, except inventories, which always start empty.

Blocks and entities use `object:get_metadata("field")`, `set_metadata("field", value)`, and `reset_metadata("field")`. Inventory fields also expose `object:get_inventory("field")`. Returned tables are copies; use setters or inventory methods to change stored values.

Block metadata saves with the chunk. Changing a block's ID clears its metadata. Entity persistence is described under [Entities](#entities).

### Player metadata

Declare fields before players join, then access them as `namespace:field`:

```lua
midless.define_player_metadata("my_mod", {
    {name = "air", type = "uint", bits = 5, default = 20},
    {name = "level", type = "uint", bits = 8, default = 1,
     on_change = function(player, old, new)
         player:send_message("Level: " .. new)
     end},
})
-- Inside a player callback:
-- player:set_metadata("my_mod:air", 10)
-- player:reset_metadata("my_mod:air")
```

Player metadata saves with inventories. `midless` is reserved for built-in fields. HP already exists as `midless:hp`; do not redeclare it.

Field `on_change` callbacks and registered listeners run after successful changes, in registration order. Unchanged values, reading defaults, and loading saves do not trigger them. `register_on_hp_change` listens to `midless:hp`, including changes through metadata setters and resets.

## Inventories and crafting

### Inventory methods

Get an inventory with `player:get_inventory()`, `player:get_inventory(name)`, or `block:get_inventory(field)` / `entity:get_inventory(field)`.

```lua
local inv = player:get_inventory()
inv:set_stack(1, {id = "midless:stone", count = 32})
local stack = inv:get_stack(1) -- Copy, or nil.
inv:set_stack(1, nil)
local added = inv:add_item({id = "midless:stone", count = 100})
```

Slots start at 1. The main player inventory uses 1–27 for storage and 28–36 for the hotbar.

`add_item` fills matching stacks, then empty slots, splitting amounts as needed. The main inventory prefers empty hotbar slots. It returns `true` if everything fits, otherwise `false` without changes.

### Screens

Register one player inventory screen. Its callback runs each time the player presses E:

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

Named player inventories start empty and use the same inventory methods.

For a chest, declare an inventory metadata field and open a screen on interaction:

```lua
midless.define_block("my_mod:chest", {
    name = "Chest", textures = {all = 4},
    metadata = {{name = "items", type = "inventory", slots = 27}},
    on_interact = function(player, chest)
        player:show_inventory({
            title = "Chest", block = chest, width = 9, height = 8,
            elements = {
                {type = "inventory", inventory = chest:get_inventory("items"),
                 x = 0, y = 0, columns = 9, rows = 3},
                {type = "label", text = "Inventory", x = 0, y = 3.5},
                {type = "inventory", inventory = player:get_inventory(),
                 x = 0, y = 4, columns = 9, rows = 4},
            },
        })
    end,
})
```

Layouts use slot-sized units and scale with the window. Include the main inventory, display each inventory slot once, and keep grids inside the screen without overlap. Set `block` for block inventories; a screen supports one block inventory.

| Element `type` | Fields |
| --- | --- |
| `inventory` | `inventory`, `slot` (first slot, default 1), `columns`, `rows`, `x`, `y` |
| `label` | `text`, `x`, `y` |
| `crafting_output` | `inventory`, `recipes` (group), input `columns`/`rows`, `x`, `y` |
| `progress` | `value`, `max`, `x`, `y`, `width`, `height` |

A crafting output occupies one slot; its input must match a displayed block or named player inventory, no larger than 3×3. A progress bar's `value` can be a fixed number or a numeric block metadata field name, which updates automatically.

### Recipes

```lua
midless.define_recipe({
    group = "crafting", -- Default.
    pattern = {{4, 4}, {4, 4}},
    output = {id = 202, count = 1},
})
midless.define_recipe({
    ingredients = {10}, -- Shapeless: one log in any input slot.
    output = {id = 4, count = 4},
})
```

- **Shaped:** up to 3×3, equal-width rows, `0` for empty cells. May shift within the input grid, but does not rotate or mirror. Other cells must be empty.
- **Shapeless:** up to 9 entries, each requiring a separate occupied slot, even for repeated IDs.
- Each ingredient cell consumes one item. The result must fit one stack. The first registered matching recipe in the chosen group wins.

Use `{id = "my_mod:gem", metadata = {quality = 2}}` as an ingredient to match metadata. Output stacks may also contain metadata. Mods can add recipes to shared groups.

### Slot rules and transactions

Inventory metadata can restrict player clicks and shift-clicks:

```lua
{name = "items", type = "inventory", slots = 3, rules = {
    [1] = {items = {6}},     -- Sand input.
    [2] = {items = {4, 10}}, -- Fuel.
    [3] = {insert = false}, -- Take-only output.
}},
```

Unrestricted slots accept any item. To position slots separately, use multiple `inventory` elements with different `slot` values and `columns = 1, rows = 1`.

Apply a block inventory operation atomically:

```lua
local ok = block:inventory_transaction("items", {
    take = {{slot = 1, id = 6, count = 1}},
    give = {{slot = 3, id = 14, count = 1}},
})
```

Returns `true` on success, or `false` without changes if input is missing or output does not fit. `take` and `give` are each optional. Combine this with [block timers](#block-callbacks-and-timers) for furnaces and machines.

## Entities

### Definition and lifecycle

```lua
local function initialize(self)
    self.timer = 0 -- Temporary Lua state.
end
midless.define_entity("my_mod:creature", {
    model = "humanoid", hp = 10, save = true,
    body = {
        enabled = true,
        min = {x = -0.3, y = 0, z = -0.3},
        max = {x = 0.3, y = 1.5, z = 0.3},
        gravity_scale = 1,
    },
    metadata = {{name = "age", type = "uint", default = 0}},
    on_spawn = initialize,
    on_load = initialize,
    on_step = function(self, dt)
        self.timer = self.timer + dt
    end,
})
local entity = midless.spawn_entity("my_mod:creature", {x = 0, y = 80, z = 0})
```

Callbacks share a `self` table; `self.object` is the entity handle. Body bounds use blocks. Optional registration fields include `texture`, `population_group`, and `despawn`.

| Callback | When |
| --- | --- |
| `on_spawn(self)` | New entity created. |
| `on_load(self)` | Saved entity restored; metadata is already available. |
| `on_step(self, dt)` | Simulation update; `dt` is seconds. |
| `on_damage(self, amount, context)` | Before damage. |
| `on_death(self, context)` | Once at zero HP, before queued removal. |
| `on_remove(self)` | Entity removed; not called for chunk unload. |
| `on_unload(self)` | Chunk unloads, before the handle becomes invalid. |

Ordinary entities save by default. Metadata, position, rotation, velocity, body settings, model, held block, nametag, and texture override persist. Ordinary `self` fields do not; rebuild them in `on_load`.

Loaded entities get new runtime IDs, invalidating old handles. `save = false` discards entities on unload, including older saved copies.

### Methods

| Call | Result or action |
| --- | --- |
| `entity:get_id()` / `entity:get_name()` | Runtime ID / registered definition name. |
| `entity:is_valid()` | Whether the handle still exists. |
| `entity:get_position()` | Position. |
| `entity:set_position(pos)` | Set position only when physics is disabled. |
| `entity:teleport(pos)` | Instant repositioning. |
| `entity:get_rotation()` / `entity:set_rotation(rotation)` | XYZ Euler radians: pitch, yaw, roll. |
| `entity:get_velocity()` / `entity:set_velocity(v)` | Velocity in blocks/second; do not multiply by `dt`. Setting it replaces steering. |
| `entity:apply_impulse(v)` | Add velocity. |
| `entity:set_move_direction(direction, speed, acceleration)` | Horizontal steering; ignores Y and normalizes direction. |
| `entity:jump(speed)` | Jump if grounded and ready; otherwise `false`. |
| `entity:is_recovering()` | Knockback recovery active. |
| `entity:get_hp()` / `entity:set_hp(hp)` | Health. |
| `entity:damage(amount, context)` | Apply damage; returns HP lost. |
| `entity:set_model(name)` | Change model. |
| `entity:get_texture()` / `entity:set_texture(name)` | Texture override; `nil` clears it. |
| `entity:set_nametag(options)` | Change nametag. |
| `entity:remove()` | Remove entity. |

Entities also expose the [metadata](#metadata) and [inventory](#inventories-and-crafting) methods.

Movement speed is 0–20 blocks/s; acceleration defaults to 32 and must be >0–100 blocks/s². Jump speed defaults to 7 and must be >0–20 blocks/s. A zero movement direction and speed stop walking. Navigation helpers are under [Mobs and spawning](#mobs-and-spawning).

### Damage

Set `hp` in the entity definition to enable health (integer, up to 65535). Omitted or zero definition HP means the entity cannot take damage.

```lua
local lost = target:damage(2, {
    attacker = player,
    cause = "melee",
    knockback = {horizontal = 8, upward = 4},
})
```

Players and entities accept this context. `attacker` is an optional player/entity handle. `cause` is a custom string with no built-in effect. Knockback components default to 0 and must be finite, 0–20 blocks/s.

Entity `on_damage` and `register_on_player_damage` run before subtraction. Return `nil` to keep damage, `false`/0 to cancel, or an integer 0–65535 to replace it. At zero entity HP, `on_death` runs once while methods remain readable, then removal is queued.

### Raycasts, nearby objects, and attacks

```lua
local hit = midless.raycast(from, to, {entities = true, ignore = entity})
local entities = midless.get_entities_in_radius(pos, 32)
local players = midless.get_players_in_radius(pos, 32)
local nearest = midless.get_player_in_radius(pos, 32)
```

Raycasts return the nearest `entity`, `player`, `block`, `unloaded`, or `nothing` in `hit.type`, plus `position`, `normal`, and `distance`. Hits expose the matching `hit.entity`, `hit.player`, or `hit.block`.

Rays are limited to 128 blocks. `entities` defaults to `true`; set `false` for terrain only. `ignore` accepts a non-player entity; use `ignore_player = player:get_id()` for a player.

The singular `get_player_in_radius` returns the nearest living, connected, movement-ready player or `nil`; radius is 0–128.

```lua
midless.register_on_player_attack(function(player, target)
    if target.type == "entity" then
        target.entity:damage(2, {attacker = player, cause = "melee"})
        return true
    end
end)
```

Attacks cast from the player's eye with 4.5-block reach and a 0.4-second cooldown, including misses. Targets use the raycast format. Return `true` to stop later handlers. The base-game mob library already handles mob attacks; avoid registering duplicate damage for the same targets.

## Mobs and spawning

### Register a mob

`midless.register_mob` uses three required callbacks: decisions, movement, and attacks.

```lua
midless.register_mob("my_mod:wanderer", {
    model = "humanoid", hp = 8, save = true,
    body = {
        enabled = true,
        min = {x = -0.3, y = 0, z = -0.3},
        max = {x = 0.3, y = 1.5, z = 0.3},
        gravity_scale = 1,
    },
    brain = {
        interval = 0.2,
        update = function(self, dt)
            return {goal = self.object:wander_goal(6)}
        end,
    },
    movement = {
        update = function(self, dt, intent)
            self.object:follow_ground_path(intent.goal, {speed = 3})
        end,
    },
    attack = {perform = function(self, target) return false end},
})
```

Mobs accept entity fields and lifecycle callbacks, but reject `on_step`. Set `save = true` to save a mob. `midless.register_mob` replaces the former `mobs.register` API.

| Callback / setting | Behavior |
| --- | --- |
| `brain.update(self, dt)` | First physics step, then every `brain.interval` seconds (default 0.2; range 1/60–60). First `dt` is 0. |
| Returned intent | `nil` or `{goal = pos, target = handle, attack = boolean}`. All fields optional; attack defaults false. |
| `movement.update(self, dt, intent)` | Every 1/60-second physics step before integration; chooses all movement. |
| `movement.during_recovery` | Default recovery suspends movement and clears steering. `true` ends recovery to allow custom movement immediately. |
| `attack.perform(self, target)` | Applies the attack effect. Must return `true` to start cooldown or `false` to retry no sooner than the brain interval. |
| `attack.range` | Default 1.8; range 0–128 blocks, measured between origins in 3D. |
| `attack.cooldown` | Default 1; range 1/60–60 seconds. |
| `attack.requires_line_of_sight` | Default `true`; checks terrain between body centers. |

A target does not imply a goal or attack. Attacks require explicit intent, a valid target with positive HP, range, cooldown, and optional visibility. The callback supplies damage, projectiles, or other effects; none are automatic.

Intent is copied; editing the movement snapshot does not change stored intent. Invalid targets are cleared and attacks disabled, while explicit goals remain until the next decision. Check `is_valid()` before using retained handles.

Recovery does not disable attacks; omit attack intent if needed. The brain reconsiders immediately after landing. Removing or killing a mob stops later callbacks. Callback errors or invalid returns are logged and remove the mob. Simulation catch-up is limited to six physics steps per server update.

Only declared metadata and normal entity fields save. Goals, paths, cooldowns, and custom `self` state must be rebuilt after load.

### Navigation helpers

These work on ordinary entities too:

| Call | Behavior |
| --- | --- |
| `midless.find_path(entity, destination)` | Ordered ground cells including start and destination, or `nil` if no complete path fits the limits. |
| `midless.can_walk_to(entity, destination)` | Check a straight, level ground route up to 32 blocks. |
| `entity:follow_ground_path(goal, options)` | Follow loaded ground with diagonal paths and automatic jumps. `nil` clears the route and stops horizontal movement. Returns `true` while pursuing a waypoint, otherwise `false`. |
| `entity:wander_goal(radius)` | Random nearby goal or `nil` during a pause. Default radius 6; range 1–16. Does not move by itself. |
| `entity:steer_toward(goal, speed, acceleration)` | Acceleration-limited 3D steering, slowing near the goal. Default acceleration 32. Set `body.gravity_scale = 0` to hover. |
| `entity:try_teleport(destination, require_ground)` | Returns `false` for blocked, liquid, or unloaded destinations. Success teleports and resets velocity. Ground requirement defaults false. |

Ground-path options: `{speed = 2, acceleration = 32, jump = 7}`. Speed is 0–20, acceleration >0–100, jump 0–20; zero jump disables jumping. Paths ignore moving obstacles, share two searches per server update, normally replan once per second, and use local legs for distant goals. Stable-goal shortcuts are checked at most five times per second.

Wandering initially pauses 2–5 seconds, pursues each goal for up to six seconds, then pauses again. 3D steering uses body collision but does not plan flight routes or avoid obstacles in advance; `set_move_direction` switches back to horizontal steering.

`try_teleport` checks only the destination, ignores other entities, and optionally requires support under the full footprint. Climbing and projectiles need custom callbacks.

### Natural spawning

Register rules after the entity and ground blocks exist, or use `register_on_ready`:

```lua
midless.register_spawn("my_mod:wanderers", {
    entity = "my_mod:wanderer", -- Requires an enabled physics body.
    interval = 5, attempts = 8, chance = 0.25,
    distance = {min = 24, max = 64},
    placement = {
        type = "ground", vertical_range = 16, avoid_liquids = true,
        ground_blocks = {"midless:grass", "midless:dirt"}, -- Optional.
    },
    population = {local_limit = 8, local_radius = 64, global_limit = 64},
    can_spawn = function(pos, context)
        -- context.rule and context.player_id identify the rule and nearby player.
        return true -- Exactly true allows spawning.
    end,
})
```

Shown numbers and booleans are defaults. Only `entity` is required. Rules have unique, nonempty names of at most 64 bytes, with at most 128 rules. There are no time or light conditions yet.

| Setting | Limits / behavior |
| --- | --- |
| `interval` | 0.1–86400 seconds. |
| `attempts` | 1–64 candidates per eligible player per interval. |
| `chance` | 0–1, applied after placement and cap checks. |
| `distance` | `min < max`; max ≤1024. Final 3D distance must meet min from every eligible player and max from at least one. |
| `placement.vertical_range` | 1–64 blocks around the player's elevation. |
| `placement.ground_blocks` | Optional whitelist; otherwise any suitable solid collision face. |
| `population.group` | Must match entity `population_group`; defaults to that group, or counts by definition if ungrouped. |
| `population.local_radius` | 1–2048 blocks around the candidate. |
| `population.local_limit` / `global_limit` | 1–1028 each. Global counts active entities, not unloaded saves. |

Spawning uses loaded chunks only. Bodies must fit, avoid physical entities/players, and have their full footprint supported by one collision face. Partial blocks can support them. `avoid_liquids` rejects liquid in occupied cells.

Work rotates across players and rules, with at most four column searches per tick. Stalls do not accumulate extra rounds; heavy load can reduce spawn frequency. Keep `can_spawn` short: it runs synchronously after built-in checks. Errors reject the candidate; distance, caps, clearance, and support are rechecked afterward. Success runs `on_spawn`.

Manual and restored entities count toward caps; pending removals do not. Rules sharing a population must use identical caps and local radius. Manual `spawn_entity` calls do not enforce caps.

Optional entity/mob fields control cleanup:

```lua
population_group = "hostile",
despawn = {distance = 96, delay = 30},
save = false,
```

`despawn` removes the entity after it stays farther than the distance from every eligible player for the delay, including when no players exist. Returning resets the timer; removal calls `on_remove`. Distance is >0–1000000; delay defaults to 30 and is 0–86400 seconds. The timer resets on load. Omit `despawn` to disable it. `save = false` separately discards entities on unload; spawn rules imply neither setting.

## Textures and models

### Textures

```lua
midless.define_texture("my_mod:skin", "textures/skin.png")
midless.define_texture("my_mod:terrain", "textures/terrain.png")
midless.set_terrain_texture("my_mod:terrain")
midless.set_terrain_texture("terrain") -- Restore default.
```

PNG only; at most 64 custom textures, each up to 1024×1024. Terrain atlases must be 256×256. `"terrain"` and `"humanoid"` are built in. Redefine a texture name to update models using it.

### Box models

```lua
midless.define_entity_model("my_mod:cube", {
    name = "Cube", texture = "terrain",
    parts = {{
        role = model.part.NONE,
        position = {0, 0, 0}, min = {-4, 0, -4}, max = {4, 8, 4},
        uv = {
            east = {0, 0, 16, 16}, west = {0, 0, 16, 16},
            up = {0, 0, 16, 16}, down = {0, 0, 16, 16},
            north = {0, 0, 16, 16}, south = {0, 0, 16, 16},
        },
    }},
})
```

16 model units equal one block. UVs are `{x, y, width, height}` in image pixels.

Part roles use `model.part.NONE`, `HEAD`, `RIGHT_ARM`, `LEFT_ARM`, `RIGHT_LEG`, or `LEFT_LEG` with the same prefix. Roles enable normal player animation.

The first right-arm part holds the selected block at its bottom center. Override with `grip = {-1.25, -9, -0.5}`, in model units relative to the part pivot.

`midless.remove_entity_model(name)` removes a model; `entity:set_model(name)` or `player:set_model(name)` assigns one.

### Variants and individual skins

Copy a model with an optional new texture and display name:

```lua
midless.define_entity_model("my_mod:skeleton", {
    base = "humanoid", texture = "my_mod:skin",
})
entity:set_texture("my_mod:skin")
player:set_texture("my_mod:skin")
entity:set_texture(nil) -- Restore current model's texture.
```

`base` accepts `"humanoid"`, a custom model name, or numeric ID. Variants copy parts, UVs, roles, first-person visibility, and grips at registration; later base changes do not propagate. `name` and `texture` otherwise inherit. Do not combine `base` with `parts`. Variants use custom model slots 1–255.

Individual overrides can also be set with `texture` in entity/mob definitions. They persist through model changes and saves, affect only that object, and replicate to clients, including the player's first-person model. `get_texture()` returns the override name or `nil`.

Textures must be registered or built in and match the model's pixel UV layout. A missing, downloading, or undersized texture falls back to the model default until compatible; saved missing names are retained. Overrides require client/server protocol 21.

## World generation

Define generation while mods load; the engine generates chunks. Without a generator mod, the world is flat at Y=64. Changes never regenerate existing chunks.

### Setup and fields

```lua
local wg = midless.worldgen
local f = wg.field
wg.configure({
    id = "my_mod:world", version = 1,
    min_y = -128, max_y = 256, sea_level = 48,
    temperature = f.noise2d({frequency = 0.001, seed_offset = 10}),
    moisture = f.noise2d({frequency = 0.001, seed_offset = 20}),
})
```

Fields calculate values from coordinates and support normal `+`, `-`, `*`, `/`, and `%` arithmetic.

| Fields | Purpose |
| --- | --- |
| `f.x()`, `f.y()`, `f.z()` | World coordinates. |
| `f.min(a,b)`, `f.max(a,b)`, `f.abs(a)` | Min, max, absolute value. |
| `f.floor(a)`, `f.ceil(a)`, `f.trunc(a)` | Rounding. |
| `f.sin(a)`, `f.cos(a)` | Trigonometry. |
| `f.lt(a,b)`, `f.eq(a,b)` | Conditions returning 0 or 1. |
| `f.select(condition, yes, no)` | Conditional value. |
| `f.noise2d(options)`, `f.noise3d(options)` | Noise fields. |
| `f.step()`, `f.steps()` | Current/total stroke steps. |
| `f.origin_x()`, `f.origin_y()`, `f.origin_z()` | Feature origin. |

Noise defaults: `type = "opensimplex2s"`, `fractal = "fbm"`, `frequency = 0.01`, `octaves = 3`, `lacunarity = 2`, `gain = 0.5`, `seed_offset = 0`.

Noise types: `opensimplex2`, `opensimplex2s`, `cellular`, `perlin`, `value_cubic`, `value`. Fractals: `none`, `fbm`, `ridged`, `pingpong`.

For density terrain, add these fields to `wg.configure`:

```lua
density = f.noise3d({frequency = 0.02}) - f.y() / 100,
caves = f.noise3d({frequency = 0.04}),
```

Positive density is solid; zero/negative is empty. Positive cave values carve terrain.

### Biomes

```lua
wg.define_biome("my_mod:highlands", {
    temperature = -0.4, moisture = 0.5,
    height = 90, height_variation = 45,
    height_noise = f.noise2d({frequency = 0.008, octaves = 5}),
    top = 3, filler = 2, filler_depth = 3, stone = 1, underwater = 6,
})
```

Temperature and moisture select the biome. Height fields shape terrain. `top` is the surface, `filler` is the layer beneath it, `filler_depth` its thickness, `stone` the underground material, and `underwater` the underwater surface.

### Ores and material rules

```lua
wg.define_ore("my_mod:iron", {
    block = 19, replaces = {1}, min_y = -64, max_y = 48,
    distribution = "veins", size = 12, spacing = 16, chance = 0.7,
    biome = "my_mod:highlands", -- Optional restriction.
})
```

Distributions: `clusters` (round deposits), `veins` (random walks), `layers` (horizontal), or `noise`. Noise distribution uses `noise = f.noise3d({...})` and `threshold = 0.6`.

Material rules run before ores and structures:

```lua
wg.define_rule({match = 3, block = 2, offset_y = -1})
wg.define_rule({match = 3, when = f.lt(f.y(), 50), block = 2})
```

### Structures and trees

```lua
wg.define_structure("my_mod:ruin", {
    blocks = {
        {x = 0, y = 0, z = 0, block = 1},
        {x = 0, y = 1, z = 0, block = 1},
        {x = 1, y = 0, z = 0, block = 1},
    },
    spacing = 160, chance = 0.3, min_y = 49, max_y = 256, rotate = true,
})
wg.define_structure("my_mod:tree", {
    tree = {height = 7, radius = 3, trunk = 10, leaves = 11},
    spacing = 32, chance = 0.4,
})
```

Blocks are relative to the structure origin; block `0` carves air. Single-block structures can place flowers and decorations.

| Option | Meaning |
| --- | --- |
| `spacing`, `chance` | Distance between placement areas and placement probability. |
| `min_y`, `max_y`, `biome` | Height and biome restrictions. |
| `max_slope` | Maximum terrain height difference. |
| `rotate` | Random 90-degree rotation. |
| `air_only` | Replace only air. |
| `foundation`, `foundation_depth` | Foundation block and maximum depth. |

### Procedural features

Use `stroke` to draw spheres along a direction and `sphere` for a single sphere:

```lua
wg.define_feature("my_mod:pillars", {
    when = f.eq(f.y(), 65) * f.eq(f.x() % 16, 0) * f.eq(f.z() % 16, 0),
    commands = {
        {op = "stroke", from = 0, to = 1, block = 1,
         steps = 8, dx = 0, dy = 1, dz = 0, radius = 1.5, bounds = 2},
        {op = "sphere", from = 1, block = 4, radius = 2.5, bounds = 3},
    },
})
```

Position slot `0` is the feature origin. Use `to` to save a stroke endpoint and `from` to continue there. Stroke fields can use `f.step()` and `f.steps()` to vary along the shape.

## Vectors

| Call | Result |
| --- | --- |
| `vector.new(x, y, z)` | New vector. No arguments makes zero; passing a vector copies it. |
| `vector.add(a, b)` / `vector.subtract(a, b)` | Sum / difference. |
| `vector.multiply(v, number)` | Scaled vector. |
| `vector.length(v)` / `vector.distance(a, b)` | Length / distance. |
| `vector.normalize(v)` | Normalized vector. |
| `vector.direction(from, to)` | Direction between positions. |
| `vector.dot(a, b)` / `vector.cross(a, b)` | Dot / cross product. |

Position three blocks in front of a player:

```lua
local pos = vector.add(player:get_eye_position(),
    vector.multiply(player:get_look_direction(), 3))
```