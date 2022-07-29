#include "MyStrategy.hpp"
#include <exception>
#include <variant>
#include <chrono>

const int UNIT_TTL = 20;
const int LOOT_TTL = 300;
model::Constants* MyStrategy::constants_;

model::Constants* MyStrategy::getConstants() {
    return constants_;
}

MyStrategy::MyStrategy(const model::Constants &consts) : constants(consts), simulator(consts) {
    MyStrategy::constants_ = &constants;
    delta_time = 1.0 / constants.ticksPerSecond;
    simulator.delta_time = delta_time;
    for (auto& obstacle: constants.obstacles) {
        obstacle.radius_sq = sqr(obstacle.radius);
    }
}

model::Order MyStrategy::getOrder(model::Game &game, DebugInterface *dbgInterface) {
    auto t_start = std::chrono::system_clock::now();
    default_dir.rotate(M_PI / 2000);

    simulator.started_tick = game.currentTick;
    debugInterface = dbgInterface;
    my_units.clear();

    if (debugInterface) {
        debugInterface->setAutoFlush(false);
        debugInterface->clear();
    }

    std::unordered_map<int, model::UnitOrder> actions;
    for (auto &unit : game.units) {
        if (unit.playerId != game.myId) {
            enemies.insert_or_assign(unit.id, unit);
            enemies.at(unit.id).ttl = UNIT_TTL;
            enemies.at(unit.id).unit_radius_sq = constants.unitRadius * constants.unitRadius;
        }
    }

    for (auto &sound : game.sounds) {
        if (constants.sounds[sound.typeIndex].name != "Steps") continue;
        int fake_id = -(rand() % 1000000);
        model::Unit fake_enemy(fake_id, -1, 100, 50, 0, sound.position, 0, {0, 0}, {0, 0}, 0, {}, 0, {}, 0, {}, 0);
        enemies.insert_or_assign(fake_enemy.id, fake_enemy);
        enemies.at(fake_id).ttl = UNIT_TTL - 2;
        enemies.at(fake_id).unit_radius_sq = constants.unitRadius * constants.unitRadius;
    }

    for (auto &projectile : game.projectiles) {
        bullets.insert_or_assign(projectile.id, projectile);
    }

    for (auto &loot : game.loot) {
        loots.insert_or_assign(loot.id, loot);
        loots.at(loot.id).ttl = LOOT_TTL;
    }

    busy_loot.clear();

    int i = 0;
    for (model::Unit &myUnit : game.units) {
        if (myUnit.playerId != game.myId)
            continue;

        myUnit.index = i;
        myUnit.unit_radius_sq = constants.unitRadius * constants.unitRadius;

        if (!myUnit.remainingSpawnTime.has_value())
            my_units.emplace_back(&myUnit);

        if (debugInterface) {
            for (auto &[key, projectile] : bullets) {
                if (projectile.intersectUnit(myUnit, constants)) {
                    debugInterface->addPolyLine({projectile.position, projectile.position + projectile.velocity * projectile.lifeTime }, 0.1, debugging::Color(0, 0.3, 0.6, 1));
                }
            }
        }

        auto unitOrder = getUnitOrder(myUnit, game.zone);
        actions.insert({ myUnit.id, unitOrder });
        ++i;
    }

    std::vector<model::Obstacle*> obstacles;
    for (auto &obstacle : constants.obstacles) {
        if (obstacle.canShootThrough) {
            continue;
        }
        for (auto& myUnit : my_units) {
            auto dist = myUnit->position.distTo(obstacle.position) - obstacle.radius;
            if(2 * dist <= constants.viewDistance) {
                obstacles.push_back(&obstacle);
                break;
            }
        }
    }

    std::vector<int> destroyed_ids;
    for (auto& [key, bullet] : bullets) {
        bool destroyed = false;
        for (auto& obstacle : obstacles) {
            if ((bullet.position.distTo(obstacle->position) - obstacle->radius) / constants.weapons[bullet.weaponTypeIndex].projectileSpeed > delta_time) {
                continue;
            }
            if (bullet.hasHit(*obstacle)) {
                destroyed = true;
                break;
            }
        }

        if (destroyed) {
            destroyed_ids.emplace_back(key);
            continue;
        }

        for (auto& unit : my_units) {
            if (bullet.hasHit(*unit)) {
                destroyed = true;
                break;
            }
        }
        bullet.position += bullet.velocity * delta_time;
        bullet.lifeTime -= delta_time;

        if (destroyed || bullet.lifeTime <= 0) {
            destroyed_ids.emplace_back(key);
            continue;
        }
    }

    for (auto& id: destroyed_ids) {
        bullets.erase(id);
    }

    destroyed_ids.clear();
    for (auto& [key, enemy] : enemies) {
        enemy.ttl--;
        if (enemy.ttl == 0) {
            destroyed_ids.emplace_back(key);
            continue;
        }
        enemy.position += enemy.velocity * delta_time;
    }

    for (auto& id: destroyed_ids) {
        enemies.erase(id);
    }

    destroyed_ids.clear();

    for (auto& [key, loot] : loots) {
        loot.ttl--;
        if (loot.ttl == 0) {
            destroyed_ids.emplace_back(key);
        }
    }

    for (auto& [unit_id, order] : actions) {
        if (!order.action || !std::holds_alternative<model::Pickup>(*order.action)) continue;
        model::Unit* myUnit = nullptr;
        for (auto unit: my_units) {
            if (unit->id == unit_id) myUnit = unit;
        }
        if (myUnit && !myUnit->action && myUnit->aim < 1e-9 && busy_loot.count(myUnit->id)) {
            destroyed_ids.emplace_back(busy_loot.at(myUnit->id));
        }
    }

    for (auto& id: destroyed_ids) {
        if (loots.count(id)) loots.erase(id);
    }

    if (debugInterface) {
        for (auto &[key, enemy] : enemies) {
            debugInterface->addRing(enemy.position, constants.unitRadius, 0.1, debugging::Color(1, 0, 0, 0.8));
            debugInterface->addPlacedText(enemy.position, std::to_string(key), {0, -1}, 0.3, debugging::Color(0, 0, 0, 0.5));
        }
        debugInterface->flush();
    }

    auto t_end = std::chrono::system_clock::now();

    elapsed_time += std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    // std::clog << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count() << " ms" << std::endl;

    return model::Order(actions);
}

model::UnitOrder MyStrategy::getUnitOrder(model::Unit& myUnit, const model::Zone& zone) {
    std::vector<model::UnitOrder> orders;
    std::vector<const model::Obstacle*> obstacles;

    if (debugInterface) {
        myUnit.calcSpeedCircle(constants);
        myUnit.showSpeedCircle(debugInterface);
    }

    for (auto &obstacle : constants.obstacles) {
        auto dist = myUnit.position.distTo(obstacle.position) - obstacle.radius;
        if(dist < constants.viewDistance) {
            obstacles.push_back(&obstacle);
        }
    }

    model::Unit* nearest_enemy = nullptr;
    model::Unit* nearest_spawn_enemy = nullptr;
    double min_dist_to_enemy = 1e9;
    double min_dist_to_spawn_enemy = 1e9;
    for (auto &[id, enemy] : enemies) {
        auto distToEnemy = enemy.position.distToSquared(myUnit.position);

        if (enemy.remainingSpawnTime.has_value() || enemy.ttl < UNIT_TTL - 2) {
            if (distToEnemy < min_dist_to_spawn_enemy) {
                nearest_spawn_enemy = &enemy;
                min_dist_to_spawn_enemy = distToEnemy;
            }
            continue;
        }
        if (distToEnemy < min_dist_to_enemy) {
            nearest_enemy = &enemy;
            min_dist_to_enemy = distToEnemy;
        }
    }

    bool has_ammo = myUnit.weapon && myUnit.ammo[*myUnit.weapon] > 0;
    bool ready_attack = has_ammo && (myUnit.shield > 0 || myUnit.shieldPotions == 0);
    bool has_bow = myUnit.weapon && constants.weapons[*myUnit.weapon].name == "Bow";
    bool is_spawn = myUnit.remainingSpawnTime.has_value();

    if (is_spawn) {
        auto spawn_order = looting(myUnit, zone);
        if (spawn_order && myUnit.health >= constants.unitHealth) {
            return *spawn_order;
        }

        return model::UnitOrder(
            (zone.nextCenter + default_dir * 0.7 * zone.nextRadius - myUnit.position),
            model::Vec2(-myUnit.direction.y, myUnit.direction.x),
            std::nullopt
        );
    }

    for (size_t ii = 0; ii < 1; ++ii) {
        if (nearest_enemy && ready_attack && 2 * min_dist_to_enemy < constants.viewDistance * constants.viewDistance) {
            shooting(myUnit, nearest_enemy, obstacles, orders);
            continue;
        }

        if (nearest_enemy && myUnit.aim > 0.033 && myUnit.shield == 0 && myUnit.shieldPotions > 0) {
            orders.emplace_back(
                (myUnit.position - nearest_enemy->position).mul(constants.maxUnitForwardSpeed),
                model::Vec2(-myUnit.position.x, -myUnit.position.y),
                std::nullopt
            );
            continue;
        }

        auto healing_order = healing(myUnit);
        auto loot_order = looting(myUnit, zone);

        if (healing_order) {
            if (loot_order) {
                healing_order->targetDirection = loot_order->targetDirection;
                healing_order->targetVelocity = loot_order->targetVelocity;
            }

            orders.push_back(*healing_order);

            continue;
        }

        if (loot_order) {
            orders.push_back(*loot_order);
            continue;
        }

        if (nearest_spawn_enemy && min_dist_to_spawn_enemy < min_dist_to_enemy && nearest_spawn_enemy->ttl >= UNIT_TTL - 10) {
            auto bullet_position = myUnit.position + myUnit.direction * constants.unitRadius;
            double dist_to_enemy = bullet_position.distTo(nearest_spawn_enemy->position) - constants.unitRadius;
            double time_to_hit = dist_to_enemy / constants.weapons[*myUnit.weapon].projectileSpeed;
            if (ready_attack && nearest_spawn_enemy->ttl == UNIT_TTL && nearest_spawn_enemy->remainingSpawnTime && time_to_hit >= *nearest_spawn_enemy->remainingSpawnTime) {
                shooting(myUnit, nearest_spawn_enemy, obstacles, orders);
                continue;
            }
            orders.emplace_back(
                (nearest_spawn_enemy->position - myUnit.position).mul(constants.maxUnitForwardSpeed),
                (nearest_spawn_enemy->position - myUnit.position),
                std::nullopt
            );
            continue;
        }

        if (nearest_enemy && has_ammo && myUnit.health == constants.unitHealth && myUnit.shield > 0) {
            orders.emplace_back(
                (nearest_enemy->position - myUnit.position).mul(constants.maxUnitForwardSpeed),
                (nearest_enemy->position - myUnit.position),
                std::nullopt
            );
            continue;
        }

        orders.emplace_back(
            (zone.nextCenter + default_dir * 0.9 * zone.nextRadius - myUnit.position),
            model::Vec2(-myUnit.direction.y, myUnit.direction.x),
            std::nullopt
        );
    }

    std::optional<model::Vec2> loot_pos;
    if(busy_loot.count(myUnit.id)) {
        int loot_id = busy_loot.at(myUnit.id);
        for (auto& [id, loot]: loots) {
            if (id == loot_id) {
                loot_pos = loot.position;
                break;
            }
        }
    }

    for (auto& order: orders) {
        auto sim_unit(myUnit);
        auto collision = simulator.SimulateMovement(sim_unit, order, obstacles, simulator.started_tick);
        if (collision) {
            if (loot_pos && loot_pos->distToSquared(sim_unit.position) <= constants.unitRadius) continue;
            auto v = ((*collision)->position - sim_unit.position).norm();
            auto shift = model::Vec2(-v.y, v.x) * v.cross(sim_unit.velocity);
            shift.norm().mul((*collision)->radius);
            order.targetVelocity = (sim_unit.position + shift - myUnit.position).mul(constants.maxUnitForwardSpeed);
            // debugInterface->addPolyLine({sim_unit.position, sim_unit.position + shift}, 0.1, debugging::Color(0, 0, 1, 1));
            // debugInterface->addRing(sim_unit.position, constants.unitRadius, 0.1, debugging::Color(0, 0, 1, 1));
        }
    }

    if (!bullets.empty()) {
        auto initial_velocaity = myUnit.velocity.isEmpty() ? default_dir : myUnit.velocity;
        auto vel = initial_velocaity;
        for (size_t it1 = 0; it1 < 10; ++it1) {
            auto dir = myUnit.direction;
            for (size_t it2 = 0; it2 < 6; ++it2) {
                orders.emplace_back(
                    vel * constants.maxUnitForwardSpeed,
                    dir,
                    std::nullopt
                );
                dir.rotate(M_PI / 3);
            }
            vel.rotate(M_PI / 5);
        }

        vel = initial_velocaity;
        for (size_t it1 = 0; it1 < 10; ++it1) {
            auto dir = myUnit.direction;
            for (size_t it2 = 0; it2 < 6; ++it2) {
                orders.emplace_back(
                    vel * constants.maxUnitForwardSpeed,
                    dir,
                    model::Aim(false)
                );
                dir.rotate(M_PI / 3);
            }
            vel.rotate(M_PI / 5);
        }
    }

    int min_damage = 1e9;
    model::UnitOrder* best_order;

    std::vector<model::Projectile> sim_bullets;
    for (const auto &b : bullets) {
        if (b.second.position.distToSquared(myUnit.position) > sqr(b.second.lifeTime * constants.weapons[b.second.weaponTypeIndex].projectileSpeed))
            continue;
        if (b.second.shooterId == myUnit.id) continue;

        sim_bullets.emplace_back(b.second);
    }

    for (auto& order: orders) {
        auto sim_unit(myUnit);
        for (auto& b: sim_bullets) {
            b.position = bullets.at(b.id).position;
            b.lifeTime = bullets.at(b.id).lifeTime;
            b.destroyed = false;
        }

        auto damage = simulator.Simulate(sim_unit, order, sim_bullets, obstacles, zone, simulator.started_tick);
        if (damage < min_damage) {
            min_damage = damage;
            best_order = &order;
        }

        if (debugInterface) {
            debugInterface->addRing(sim_unit.position, constants.unitRadius, 0.1, debugging::Color(0, 0, 1, 1));
            debugInterface->addPlacedText(sim_unit.position, std::to_string(damage), {0, -1}, 0.3, debugging::Color(0, 0, 0, 0.5));
        }
    }

    if (debugInterface) {
        debugInterface->addPolyLine({myUnit.position, myUnit.position + best_order->targetVelocity}, 0.1, debugging::Color(1, 0, 0, 1));
        debugInterface->addPolyLine({myUnit.position, myUnit.position + best_order->targetDirection}, 0.1, debugging::Color(1, 0, 0, 1));
    }

    return *best_order;
}

void MyStrategy::shooting(
        const model::Unit& myUnit,
        const model::Unit* nearest_enemy,
        std::vector<const model::Obstacle*>& obstacles,
        std::vector<model::UnitOrder>& orders) {
    bool shooting = false;
    double aim_delta = 1.0 / constants.weapons[*myUnit.weapon].aimTime / constants.ticksPerSecond;

     model::Projectile bullet(-1, *myUnit.weapon, myUnit.id, myUnit.playerId,
        myUnit.position + myUnit.direction * constants.unitRadius,
        myUnit.direction * constants.weapons[*myUnit.weapon].projectileSpeed,
        constants.weapons[*myUnit.weapon].projectileLifeTime
    );

    double dist_to_enemy = bullet.position.distTo(nearest_enemy->position) - constants.unitRadius;
    auto enemy_hit = bullet.getHit(*nearest_enemy);
    if (enemy_hit) {
        dist_to_enemy = bullet.position.distTo(*enemy_hit);
    }

    double time_to_hit = dist_to_enemy / constants.weapons[*myUnit.weapon].projectileSpeed;
    int ticks_to_hit = std::ceil(time_to_hit * constants.ticksPerSecond);
    auto sim_enemy(*nearest_enemy);
    simulator.SimulateEnemyMovement(sim_enemy, obstacles, simulator.started_tick + 30 - ticks_to_hit);

    bool can_shoot = myUnit.nextShotTick - simulator.started_tick <= 15;

    if (fabs(1.0 - myUnit.aim) <= aim_delta && can_shoot && enemy_hit) {
        double min_dist_to_obstacle = 1e9;
        double min_dist_to_ally = 1e9;

        for (auto& obstacle: obstacles) {
            if (obstacle->canShootThrough) continue;
            auto hit = bullet.getHit(*obstacle);
            if (hit) {
                min_dist_to_obstacle = std::min(min_dist_to_obstacle, hit->distTo(bullet.position));
            }
        }

        for (auto& unit: my_units) {
            auto hit = bullet.getHit(*unit);
            if (hit) {
                min_dist_to_ally = std::min(min_dist_to_ally, hit->distTo(bullet.position));
            }
        }

        shooting = (dist_to_enemy <= min_dist_to_obstacle && dist_to_enemy <= min_dist_to_ally);
    }

    bool is_archer = constants.weapons[*myUnit.weapon].name == "Bow";
    double dist_coef = is_archer ? 9 : 18;

    auto move = (nearest_enemy->position - myUnit.position).mul(constants.maxUnitForwardSpeed);
    auto min_dist_to_enemy = nearest_enemy->position.distToSquared(myUnit.position);
    if (dist_coef * min_dist_to_enemy < constants.viewDistance * constants.viewDistance) {
        move.rotate(M_PI);
    }

    for (size_t it = 0; it < 16; ++it) {
        orders.emplace_back(
            move,
            sim_enemy.position - myUnit.position,
            can_shoot ? std::optional<model::ActionOrder>(model::Aim(shooting)) : std::nullopt
        );
        move.rotate(M_PI / 8);
    }
}

std::optional<model::UnitOrder> MyStrategy::healing(const model::Unit& myUnit) const {
    if (constants.maxShield - myUnit.shield >= constants.shieldPerPotion && myUnit.shieldPotions > 0 && !myUnit.action && myUnit.aim < 1e-8) {
        return model::UnitOrder(
            myUnit.direction * constants.maxUnitForwardSpeed,
            {-myUnit.position.x, -myUnit.position.y},
            model::UseShieldPotion()
        );
    }

    return std::nullopt;
}

std::optional<model::UnitOrder> MyStrategy::looting(const model::Unit& myUnit, const model::Zone& zone) {
    if (loots.empty()) {
        return std::nullopt;
    }

    double min_dist = 1e9;
    model::Loot* nearest_loot = nullptr;
    for (auto& [id, loot]: loots) {
        bool busy = false;
        for (auto& [key_l, l]: busy_loot) {
            if (l == id) {
                busy = true;
                break;
            }
        }

        if (busy) {
            continue;
        }

        if (zone.currentCenter.distToSquared(loot.position) >= zone.currentRadius * zone.currentRadius) {
            continue;
        }

        double min_dist_to_me = loot.position.distToSquared(myUnit.position);
        double min_dist_to_enemy = 1e9;
        for (auto& [id, enemy] : enemies) {
            min_dist_to_enemy = std::min(
                min_dist_to_enemy,
                loot.position.distToSquared(enemy.position));
        }

        if (9 * min_dist_to_enemy < constants.viewDistance * constants.viewDistance && min_dist_to_me > 2) {
            continue;
        }

        double dist_to_loot = loot.position.distToSquared(myUnit.position);
        if (dist_to_loot > min_dist) {
            continue;
        }

        if (std::holds_alternative<model::ShieldPotions>(loot.item) && myUnit.shieldPotions < constants.maxShieldPotionsInInventory && myUnit.weapon && myUnit.ammo[*myUnit.weapon] > 0) {
            nearest_loot = &loot;
            min_dist = dist_to_loot;
        }

        if (std::holds_alternative<model::Ammo>(loot.item) && myUnit.weapon) {
            auto ammo = std::get<model::Ammo>(loot.item);
            if (constants.weapons[ammo.weaponTypeIndex].name == "Magic wand") continue;
            // if (constants.weapons[ammo.weaponTypeIndex].name == "Staff" && zone.currentRadius > radius_treshold) continue;

            if (myUnit.ammo[ammo.weaponTypeIndex] < 0.9 * constants.weapons[ammo.weaponTypeIndex].maxInventoryAmmo && (ammo.weaponTypeIndex == *myUnit.weapon || myUnit.ammo[*myUnit.weapon] >= 5)) {
                nearest_loot = &loot;
                min_dist = dist_to_loot;
            }
        }

        if (std::holds_alternative<model::Weapon>(loot.item)) {
            auto weapon = std::get<model::Weapon>(loot.item);
            if (constants.weapons[weapon.typeIndex].name == "Magic wand") continue;
            // if (constants.weapons[weapon.typeIndex].name == "Staff" && zone.currentRadius > radius_treshold) continue;

            if (!myUnit.weapon ||
                (myUnit.ammo[weapon.typeIndex] > 0 && constants.weapons[*myUnit.weapon].name == "Magic wand") ||
                (constants.weapons[weapon.typeIndex].name == "Bow" && myUnit.ammo[weapon.typeIndex] > 0 && constants.weapons[*myUnit.weapon].name != "Bow") ||
                (myUnit.ammo[*myUnit.weapon] == 0 && myUnit.ammo[weapon.typeIndex] > 0)) {
                nearest_loot = &loot;
                min_dist = dist_to_loot;
            }
        }
    }

    if (!nearest_loot) {
        return std::nullopt;
    }

    auto dir = nearest_loot->position - myUnit.position;
    dir.norm();

    busy_loot.insert({myUnit.id, nearest_loot->id});
    if (min_dist >= constants.unitRadius * constants.unitRadius) {
        return model::UnitOrder(
            dir.mul(constants.maxUnitForwardSpeed),
            dir,
            std::nullopt
        );
    }

    return model::UnitOrder(
        {0, 0},
        dir,
        model::Pickup(nearest_loot->id)
    );
}

void MyStrategy::debugUpdate(int displayedTick, DebugInterface& dbgInterface) {}

void MyStrategy::finish() {
    std::cout << "Last tick " << simulator.started_tick << " -- " << "Elapsed time " << elapsed_time << " ms" << std::endl;
}
