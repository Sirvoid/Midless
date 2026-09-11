local path = ...
midless.define_texture("midless:skeleton", path .. "/textures/skeleton.png")
midless.define_entity_model("midless:skeleton", {
    base = "humanoid",
    texture = "midless:skeleton",
})

local speed, detection_range = 2.25, 36
local horizontal, upward = 8, 4
local function initialize(self)
    self.object:set_nametag({text = "&2Skeleton"})
end

midless.register_mob("midless:skeleton", {
    model = "midless:skeleton",
    hp = 10,
    save = false,
    body = {
        enabled = true,
        min = {x = -0.3, y = 0, z = -0.3},
        max = {x = 0.3, y = 1.8, z = 0.3},
        gravity_scale = 1, ground_friction = 8, restitution = 0,
    },
    on_spawn = initialize,
    on_load = initialize,
    brain = {
        interval = 0.2,
        update = function(self, dt)
            local pos = self.object:get_position()
            local target = midless.get_player_in_radius(pos, detection_range)
            if target then
                self.chasing = true
                local goal = target:get_position()
                local delta = vector.subtract(goal, pos)

                -- Stop inside melee range
                if vector.length(delta) <= 1.8 then
                    local eye = {x = 0, y = 0.9, z = 0}
                    local hit = midless.raycast(vector.add(pos, eye), vector.add(goal, eye), {entities = false})
                    if hit.type == "nothing" then goal = nil end
                end

                return {goal = goal, target = target, attack = true}
            end
            self.chasing = false
            return {goal = self.object:wander_goal(6)}
        end,
    },
    movement = {
        update = function(self, dt, intent)
            local chosen_speed

            if self.chasing then
                chosen_speed = speed
            else
                chosen_speed = speed * 0.5
            end

            self.object:follow_ground_path(intent.goal, {
                speed = chosen_speed
            })
        end
    },
    attack = {
        range = 1.8,
        cooldown = 1,
        requires_line_of_sight = true,
        perform = function(self, target)
            return target:damage(2, {
                attacker = self.object,
                cause = "melee",
                knockback = {horizontal = horizontal, upward = upward},
            }) > 0
        end,
    },
    population_group = "hostile",
    despawn = {distance = 96, delay = 15},
})

midless.register_on_player_attack(function(player, hit)
    if player:get_hp() <= 0 or hit.type ~= "entity" or hit.entity:get_name() ~= "midless:skeleton" then return false end
    hit.entity:damage(2, {
        attacker = player,
        cause = "melee",
        knockback = {horizontal = horizontal, upward = upward},
    })
    return true
end)

midless.register_spawn("midless:humanoid_monster_spawn", {
    entity = "midless:skeleton",
    interval = 5,
    attempts = 8,
    chance = 0.25,
    distance = {min = 32, max = 48},
    placement = {type = "ground", ground_blocks = {"midless:grass", "midless:dirt", "midless:stone"}, vertical_range = 16, avoid_liquids = true},
    population = {group = "hostile", local_limit = 8, local_radius = 64, global_limit = 64},
})
