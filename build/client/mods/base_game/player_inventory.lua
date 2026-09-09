midless.define_player_inventory("base_game:crafting", {slots = 4})

midless.define_player_inventory_screen(function(player)
    local crafting = player:get_inventory("base_game:crafting")

    return {
        title = "Inventory",
        width = 9, height = 8,
        elements = {
            {type = "inventory", inventory = crafting,
             x = 0, y = 0, columns = 2, rows = 2},
            {type = "crafting_output", recipes = "crafting", inventory = crafting,
             columns = 2, rows = 2, x = 4, y = 0.5},
            {type = "label", text = "Inventory", x = 0, y = 3.5},
            {type = "inventory", inventory = player:get_inventory(),
             x = 0, y = 4, columns = 9, rows = 4},
        },
    }
end)
