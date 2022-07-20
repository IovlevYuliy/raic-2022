#include "MyStrategy.hpp"
#include <exception>
#include <variant>
#include <chrono>

model::Constants* MyStrategy::constants_;

model::Constants* MyStrategy::getConstants() {
    return constants_;
}

MyStrategy::MyStrategy(const model::Constants &consts) : constants(consts), simulator(consts) {
    MyStrategy::constants_ = &constants;
    delta_time = 1.0 / constants.ticksPerSecond;
}

model::Order MyStrategy::getOrder(model::Game &game, DebugInterface *dbgInterface) {
    auto t_start = std::chrono::system_clock::now();
    default_dir.rotate(M_PI / 500);

    simulator.started_tick = game.currentTick;
    debugInterface = dbgInterface;
    if (debugInterface) {
        debugInterface->setAutoFlush(false);
        debugInterface->clear();
    }

    std::unordered_map<int, model::UnitOrder> actions;
    std::vector<model::Unit> enemies;
    std::vector<model::Unit> my_units;
    for (auto &unit : game.units) {
        if (unit.playerId != game.myId) {
            enemies.push_back(unit);
        }
    }

    for (auto &projectile : game.projectiles) {
        if (projectile.shooterPlayerId != game.myId) {
            bullets.insert({ projectile.id, projectile });
        }
    }

    int i = 0;
    for (model::Unit &myUnit : game.units) {
        if (myUnit.playerId != game.myId)
            continue;
        my_units.emplace_back(myUnit);
        myUnit.index = i;
        if (debugInterface) {
            for (auto &[key, projectile] : bullets) {
                if (projectile.intersectUnit(myUnit, constants)) {
                    debugInterface->addPolyLine({projectile.position, projectile.position + projectile.velocity * projectile.lifeTime }, 0.1, debugging::Color(0, 0.3, 0.6, 1));
                }
            }
        }

        auto unitOrder = getUnitOrder(myUnit, enemies, game.loot, game.zone);
        actions.insert({ myUnit.id, unitOrder });
        ++i;
    }

    if (debugInterface) {
        debugInterface->flush();
    }

    std::vector<model::Obstacle*> obstacles;
    for (auto &obstacle : constants.obstacles) {
        if (obstacle.canShootThrough) {
            continue;
        }
        for (auto& myUnit : my_units) {
            auto dist = myUnit.position.distTo(obstacle.position) - obstacle.radius;
            if(dist < constants.viewDistance) {
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
            if (bullet.hasHit(unit)) {
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

    auto t_end = std::chrono::system_clock::now();

    elapsed_time += std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    // std::clog << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count() << " ms" << std::endl;

    return model::Order(actions);
}

model::UnitOrder MyStrategy::getUnitOrder(
        model::Unit& myUnit,
        const std::vector<model::Unit>& enemies,
        const std::vector<model::Loot>& loot,
        const model::Zone& zone) const {
    std::vector<model::UnitOrder> orders;
    std::vector<model::Obstacle> obstacles;

    if (debugInterface) {
        myUnit.calcSpeedCircle(constants);
        myUnit.showSpeedCircle(debugInterface);
    }

    for (auto &obstacle : constants.obstacles) {
        auto dist = myUnit.position.distTo(obstacle.position) - obstacle.radius;
        if(dist < constants.viewDistance) {
            obstacles.emplace_back(obstacle);
        }
    }

    std::optional<model::Unit> nearest_enemy;
    double min_dist_to_enemy = 1e9;
    for (auto &enemy : enemies) {
        if (enemy.remainingSpawnTime.has_value()) {
            continue;
        }
        auto distToEnemy = enemy.position.distToSquared(myUnit.position);
        if(distToEnemy < min_dist_to_enemy) {
            nearest_enemy = enemy;
            min_dist_to_enemy = distToEnemy;
        }
    }

    bool has_ammo = myUnit.weapon && myUnit.ammo[*myUnit.weapon] > 0;
    bool has_bow = myUnit.weapon && constants.weapons[*myUnit.weapon].name == "Bow";
    bool is_spawn = myUnit.remainingSpawnTime.has_value();

    if (is_spawn) {
        auto move_to = zone.nextCenter;
        if (myUnit.index == 0) {
            move_to.x -= zone.currentRadius / 1.5;
        }
        if (myUnit.index == 1) {
            move_to.x += zone.currentRadius / 1.5;
        }
        if (myUnit.index == 2) {
            move_to.y += zone.currentRadius / 1.5;
        }
        if (myUnit.index == 3) {
            move_to.y -= zone.currentRadius / 1.5;
        }

        return model::UnitOrder(
            (move_to - myUnit.position).mul(constants.zoneSpeed),
            (move_to - myUnit.position),
            std::nullopt
        );
    }
    // simulateMovement(myUnit, threats);

    for (size_t ii = 0; ii < 1; ++ii) {
        if (nearest_enemy && has_ammo && min_dist_to_enemy < constants.viewDistance * constants.viewDistance / 2 && ((has_bow && zone.currentRadius > radius_treshold) || zone.currentRadius < radius_treshold)) {
            bool shooting = false;
            double aim_delta = 1.0 / constants.weapons[*myUnit.weapon].aimTime / constants.ticksPerSecond;

            model::Projectile bullet(-1, *myUnit.weapon, myUnit.id, myUnit.playerId, myUnit.position + myUnit.direction * constants.unitRadius, myUnit.direction * constants.weapons[*myUnit.weapon].projectileSpeed, constants.weapons[*myUnit.weapon].projectileLifeTime);
            double dist_to_enemy = bullet.position.distTo(nearest_enemy->position) - constants.unitRadius;
            bool is_archer = nearest_enemy->weapon && constants.weapons[*nearest_enemy->weapon].name == "Bow";
            double dist_coef = is_archer ? 4 : 9;
            if (fabs(1.0 - myUnit.aim) <= aim_delta) {

                double min_dist_to_obstacle = 1e9;
                std::optional<model::Obstacle> nearest_obst;

                for (auto& obstacle: constants.obstacles) {
                    if (obstacle.canShootThrough) continue;
                    if (!bullet.intersectCircle(obstacle.position, obstacle.radius)) continue;
                    double dist_to_obst = obstacle.position.distTo(bullet.position) - obstacle.radius;
                    if (dist_to_obst < min_dist_to_obstacle) {
                        nearest_obst = obstacle;
                        min_dist_to_obstacle = dist_to_obst;
                    }
                }

                shooting = (!nearest_obst || dist_to_enemy <= min_dist_to_obstacle);
            }

            double time_to_hit = dist_to_enemy / constants.weapons[*myUnit.weapon].projectileSpeed;
            auto move = (nearest_enemy->position - myUnit.position).mul(constants.maxUnitForwardSpeed);
            if (min_dist_to_enemy < constants.viewDistance * constants.viewDistance / dist_coef) {
                move.rotate(M_PI_2);
            }

            for (size_t it = 0; it < 16; ++it) {
                orders.push_back(model::UnitOrder(
                    move,
                    (nearest_enemy->position + nearest_enemy->velocity * time_to_hit) - myUnit.position,
                    model::Aim(shooting)
                ));
                move.rotate(M_PI / 8);
            }

            continue;
        }

        auto healing_order = healing(myUnit);
        auto loot_order = looting(loot, myUnit, zone);

        if (healing_order) {
            if (loot_order) {
                healing_order->targetDirection = loot_order->targetDirection;
                healing_order->targetVelocity = loot_order->targetVelocity;
            }

            for (size_t it = 0; it < 8; ++it) {
                orders.push_back(*healing_order);
                healing_order->targetDirection.rotate(M_PI / 4);
            }

            continue;
        }

        if (loot_order) {
            orders.push_back(*loot_order);
            continue;
        }

        if (nearest_enemy && has_ammo && myUnit.health == constants.unitHealth && myUnit.shield > 0) {
            orders.push_back(model::UnitOrder(
                (nearest_enemy->position - myUnit.position).mul(constants.maxUnitForwardSpeed),
                (nearest_enemy->position - myUnit.position),
                std::nullopt
            ));
            continue;
        }

        orders.push_back(model::UnitOrder(
            (zone.nextCenter + default_dir * 0.9 * zone.nextRadius - myUnit.position),
            {-myUnit.direction.y, myUnit.direction.x},
            std::nullopt
        ));
    }

    auto initial_velocaity = myUnit.velocity.isEmpty() ? default_dir : myUnit.velocity;

    auto vel = initial_velocaity;
    for (size_t it1 = 0; it1 < 16; ++it1) {
        auto dir = myUnit.direction;
        for (size_t it2 = 0; it2 < 8; ++it2) {
            orders.push_back(model::UnitOrder(
                vel * constants.maxUnitForwardSpeed,
                dir,
                std::nullopt
            ));
            dir.rotate(M_PI / 4);
        }
        vel.rotate(M_PI / 8);
    }

    vel = initial_velocaity;
    for (size_t it1 = 0; it1 < 16; ++it1) {
        auto dir = myUnit.direction;
        for (size_t it2 = 0; it2 < 8; ++it2) {
            orders.push_back(model::UnitOrder(
                vel * constants.maxUnitForwardSpeed,
                dir,
                model::Aim(false)
            ));
            dir.rotate(M_PI / 4);
        }
        vel.rotate(M_PI / 8);
    }

    int min_damage = 1e9;
    model::UnitOrder* best_order;
    model::Vec2 vec;

    std::vector<model::Projectile> sim_bullets;
    for (const auto &b : bullets)
        sim_bullets.emplace_back(b.second);

    for (auto& order: orders) {
        auto sim_unit(myUnit);
        for (auto& b: sim_bullets) {
            b.position = bullets.at(b.id).position;
            b.lifeTime = bullets.at(b.id).lifeTime;
            b.destroyed = false;
        }

        auto damage = simulator.Simulate(sim_unit, order, sim_bullets, enemies, obstacles, zone, simulator.started_tick);
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

    // cerr << best_order->toString() << endl;
    return *best_order;
}

void MyStrategy::simulateMovement(const model::Unit& myUnit, const std::vector<model::Projectile>& bullets) const {
    // double offset = 360.0 / 16.0;
    // for (int ang = 0; ang < 360; ang += offset) {
    //     auto dir = myUnit.direction.clone().rotate(ang);

    //     auto sim_unit(myUnit);
    //     auto sim_bullets(bullets);
    //     model::UnitOrder order(
    //         dir * constants.maxUnitForwardSpeed,
    //         sim_unit.direction,
    //         std::nullopt);

    //     auto damage = simulator.Simulate(sim_unit, order, sim_bullets, 30);

    //     if (debugInterface) {
    //         debugInterface->addRing(sim_unit.position, constants.unitRadius, 0.1, debugging::Color(0, 0, 1, 1));
    //         debugInterface->addPlacedText(sim_unit.position, std::to_string(damage), {0, -1}, 0.3, debugging::Color(0, 0, 0, 0.5));
    //     }
    // }
}

std::optional<model::Vec2> MyStrategy::dodging(std::vector<model::Projectile>& bullets, const model::Unit& myUnit) {
    // if (bullets.empty()) {
    //     return std::nullopt;
    // }

    // double offset = 360.0 / 16.0;
    // double min_damage = 1e9;
    // std::optional<model::Vec2> res;

    // for (int ang = 0; ang < 360; ang += offset) {
    //     auto dir = myUnit.direction.clone().rotate(ang);

    //     auto sim_unit(myUnit);
    //     auto sim_bullets(bullets);
    //     model::UnitOrder order(
    //         dir * constants.maxUnitForwardSpeed,
    //         sim_unit.direction,
    //         std::nullopt);

    //     auto damage = simulator.Simulate(sim_unit, order, sim_bullets, 30);
    //     if (damage < min_damage) {
    //         min_damage = damage;
    //         res = dir * constants.maxUnitForwardSpeed;
    //     }

    //     if (debugInterface) {
    //         debugInterface->addRing(sim_unit.position, constants.unitRadius, 0.1, debugging::Color(0, 0, 1, 1));
    //         debugInterface->addPlacedText(sim_unit.position, std::to_string(damage), {0, -1}, 0.3, debugging::Color(0, 0, 0, 0.5));
    //     }
    // }

    return {};
}

std::optional<model::UnitOrder> MyStrategy::healing(const model::Unit& myUnit) const {
    double aim_delta = 1.0 / constants.weapons[*myUnit.weapon].aimTime / constants.ticksPerSecond;
    if (constants.maxShield - myUnit.shield >= constants.shieldPerPotion && myUnit.shieldPotions > 0 && myUnit.ammo[*myUnit.weapon] > 0 && !myUnit.action && myUnit.aim < aim_delta) {
        return model::UnitOrder(
            myUnit.direction * constants.maxUnitForwardSpeed,
            {-myUnit.position.x, -myUnit.position.y},
            model::UseShieldPotion()
        );
    }

    return std::nullopt;
}

std::optional<model::UnitOrder> MyStrategy::looting(const std::vector<model::Loot>& loots, const model::Unit& myUnit, const model::Zone& zone) const {
    if (loots.empty()) {
        return std::nullopt;
    }

    double min_dist = 1e9;
    std::optional<model::Loot> nearest_loot;
    for (auto& loot: loots) {
        if (zone.currentCenter.distToSquared(loot.position) >= zone.currentRadius * zone.currentRadius) {
            continue;
        }

        if (loot.busy) {
            continue;
        }

        double dist_to_loot = loot.position.distToSquared(myUnit.position);
        if (dist_to_loot > min_dist) {
            continue;
        }

        if (std::holds_alternative<model::ShieldPotions>(loot.item) && myUnit.shieldPotions < constants.maxShieldPotionsInInventory - 3 && myUnit.weapon && myUnit.ammo[*myUnit.weapon] > 0 &&
            ((zone.currentRadius > radius_treshold && constants.weapons[*myUnit.weapon].name == "Bow") || zone.currentRadius < radius_treshold)) {
            nearest_loot = loot;
            min_dist = dist_to_loot;
        }

        if (std::holds_alternative<model::Ammo>(loot.item) && myUnit.health > constants.unitHealth * 0.7) {
            auto ammo = std::get<model::Ammo>(loot.item);
            if ((constants.weapons[ammo.weaponTypeIndex].name == "Magic wand" || constants.weapons[ammo.weaponTypeIndex].name == "Staff") && zone.currentRadius > radius_treshold) {
                continue;
            }

            if (myUnit.ammo[ammo.weaponTypeIndex] < 0.9 * constants.weapons[ammo.weaponTypeIndex].maxInventoryAmmo) {
                nearest_loot = loot;
                min_dist = dist_to_loot;
            }
        }

        if (std::holds_alternative<model::Weapon>(loot.item) && myUnit.health > constants.unitHealth * 0.7) {
            auto weapon = std::get<model::Weapon>(loot.item);
            if ((constants.weapons[weapon.typeIndex].name == "Magic wand" || constants.weapons[weapon.typeIndex].name == "Staff") && zone.currentRadius > radius_treshold) {
                continue;
            }

            if (!myUnit.weapon ||
                (myUnit.ammo[weapon.typeIndex] > 0 && constants.weapons[*myUnit.weapon].name == "Magic wand") ||
                (constants.weapons[weapon.typeIndex].name == "Bow" && myUnit.ammo[weapon.typeIndex] > 0 && constants.weapons[*myUnit.weapon].name != "Bow")) {
                nearest_loot = loot;
                min_dist = dist_to_loot;
            }
        }
    }

    if (!nearest_loot) {
        return std::nullopt;
    }

    auto dir = nearest_loot->position - myUnit.position;
    dir.norm();
    nearest_loot->busy = true;

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
