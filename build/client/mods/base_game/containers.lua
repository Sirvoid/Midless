local CHEST_ID = "base_game:chest"

midless.define_block(CHEST_ID, {
    name = "Chest",
    textures = {all = 4, top = 9, bottom = 9},
    hardness = 2.5, dig_group = "wood",
    metadata = {
        {name = "items", type = "inventory", slots = 27},
    },

    on_interact = function(player, block)
        player:show_inventory({
            title = "Chest",
            block = block,
            width = 9,
            height = 8,
            elements = {
                {
                    type = "inventory",
                    inventory = block:get_inventory("items"),
                    x = 0, y = 0,
                    columns = 9, rows = 3,
                },
                {type = "label", text = "Inventory", x = 0, y = 3.5},
                {
                    type = "inventory",
                    inventory = player:get_inventory(),
                    x = 0, y = 4,
                    columns = 9, rows = 4,
                },
            },
        })
    end,
})


local CRAFTING_TABLE = "base_game:crafting_table"

midless.define_block(CRAFTING_TABLE, {
    name = "Crafting Table",
    textures = {all = 4, sides = 19, top = 20},
    hardness = 2.5, dig_group = "wood",
    metadata = {
        {name = "ingredients", type = "inventory", slots = 9},
    },
    on_interact = function(player, block)
        local ingredients = block:get_inventory("ingredients")
        player:show_inventory({
            title = "Crafting Table",
            block = block,
            width = 9, height = 8,
            elements = {
                {type = "inventory", inventory = ingredients,
                 x = 0, y = 0, columns = 3, rows = 3},
                {type = "label", text = ">", x = 4, y = 1},
                {type = "crafting_output", recipes = "crafting", inventory = ingredients,
                 columns = 3, rows = 3, x = 5, y = 1},
                {type = "label", text = "Inventory", x = 0, y = 3.5},
                {type = "inventory", inventory = player:get_inventory(),
                 x = 0, y = 4, columns = 9, rows = 4},
            },
        })
    end,
})

