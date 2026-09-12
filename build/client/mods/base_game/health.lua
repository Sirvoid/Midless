local path = ...

midless.define_texture("midless:heart", path .. "/textures/heart.png")
midless.define_hud_bar("midless:health", {
    texture = "midless:heart",
    max = 20,
    icons = 10,
    priority = 0,
})

midless.register_on_player_join(function(player)
    player:set_hud_bar("midless:health", {
        value = player:get_hp(),
        visible = true,
    })
end)

midless.register_on_hp_change(function(player, old_hp, new_hp)
    if new_hp < old_hp then
        player:camera_kick({pitch = 4, roll = 8, duration = 0.3})
    end

    if new_hp <= 0 then
        player:teleport(player:get_spawn_point())
        player:set_hp(20)
        return
    end

    player:set_hud_bar("midless:health", {value = new_hp})
end)

midless.register_on_player_land(function(player, distance)
    local damage = math.max(0, math.floor(distance - 4))
    if damage > 0 then
        player:set_hp(math.max(0, player:get_hp() - damage))
    end
end)
