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
    default_dir.rotate(M_PI / 2000);

    simulator.started_tick = game.currentTick;
    debugInterface = dbgInterface;
    if (debugInterface) {
        debugInterface->setAutoFlush(false);
        debugInterface->clear();
    }

    std::unordered_map<int, model::UnitOrder> actions;
    std::vector<model::Unit*> enemies;
    std::vector<model::Unit> my_units;
    for (auto &unit : game.units) {
        if (unit.playerId != game.myId) {
            enemies.push_back(&unit);
        }
    }

    for (auto &projectile : game.projectiles) {
        if (projectile.shooterPlayerId != game.myId) {
            bullets.insert({ projectile.id, projectile });
        }
    }

    busy_loot.clear();

    int i = 0;
    for (model::Unit &myUnit : game.units) {
        if (myUnit.playerId != game.myId)
            continue;
        my_units.emplace_back(myUnit);
        myUnit.index = i;
        myUnit.unit_radius_sq = constants.unitRadius * constants.unitRadius;

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
        const std::vector<model::Unit*>& enemies,
        const std::vector<model::Loot>& loot,
        const model::Zone& zone) {
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
    double min_dist_to_enemy = 1e9;
    for (auto &enemy : enemies) {
        if (enemy->remainingSpawnTime.has_value()) {
            continue;
        }
        auto distToEnemy = enemy->position.distToSquared(myUnit.position);
        if(distToEnemy < min_dist_to_enemy) {
            nearest_enemy = enemy;
            min_dist_to_enemy = distToEnemy;
        }
    }

    bool has_ammo = myUnit.weapon && myUnit.ammo[*myUnit.weapon] > 0;
    bool has_bow = myUnit.weapon && constants.weapons[*myUnit.weapon].name == "Bow";
    bool is_spawn = myUnit.remainingSpawnTime.has_value();

    if (is_spawn) {
        auto spawn_order = looting(loot, enemies, myUnit, zone);
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
        if (nearest_enemy && has_ammo && min_dist_to_enemy < constants.viewDistance * constants.viewDistance / 2 && ((has_bow && zone.currentRadius > radius_treshold) || zone.currentRadius < radius_treshold)) {
            bool shooting = false;
            double aim_delta = 1.0 / constants.weapons[*myUnit.weapon].aimTime / constants.ticksPerSecond;

            auto bullet_position = myUnit.position + myUnit.direction * constants.unitRadius;
            double dist_to_enemy = bullet_position.distTo(nearest_enemy->position) - constants.unitRadius;
            bool is_archer = nearest_enemy->weapon && constants.weapons[*nearest_enemy->weapon].name == "Bow";
            double dist_coef = is_archer ? 4 : 9;
            bool can_shoot = myUnit.nextShotTick - simulator.started_tick <= 15;
            if (fabs(1.0 - myUnit.aim) <= aim_delta && can_shoot) {
                double min_dist_to_obstacle = 1e9;
                model::Projectile bullet(-1, *myUnit.weapon, myUnit.id, myUnit.playerId,
                    bullet_position,
                    myUnit.direction * constants.weapons[*myUnit.weapon].projectileSpeed,
                    constants.weapons[*myUnit.weapon].projectileLifeTime
                );
                const model::Obstacle* nearest_obst;

                for (auto& obstacle: obstacles) {
                    if (obstacle->canShootThrough) continue;
                    if (!bullet.intersectCircle(obstacle->position, obstacle->radius)) continue;
                    double dist_to_obst = obstacle->position.distTo(bullet.position) - obstacle->radius;
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
                orders.emplace_back(
                    move,
                    (nearest_enemy->position + nearest_enemy->velocity * time_to_hit) - myUnit.position,
                    can_shoot ? std::optional<model::ActionOrder>(model::Aim(shooting)) : std::nullopt
                );
                move.rotate(M_PI / 8);
            }

            continue;
        }

        auto healing_order = healing(myUnit);
        auto loot_order = looting(loot, enemies, myUnit, zone);

        if (healing_order) {
            if (loot_order) {
                healing_order->targetDirection = loot_order->targetDirection;
                healing_order->targetVelocity = loot_order->targetVelocity;
            }

            orders.push_back(*healing_order);
            // for (size_t it = 0; it < 8; ++it) {
            //     healing_order->targetDirection.rotate(M_PI / 4);
            // }

            continue;
        }

        if (loot_order) {
            orders.push_back(*loot_order);
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

    auto initial_velocaity = myUnit.velocity.isEmpty() ? default_dir : myUnit.velocity;

    auto vel = initial_velocaity;
    for (size_t it1 = 0; it1 < 12; ++it1) {
        auto dir = myUnit.direction;
        for (size_t it2 = 0; it2 < 8; ++it2) {
            orders.emplace_back(
                vel * constants.maxUnitForwardSpeed,
                dir,
                std::nullopt
            );
            dir.rotate(M_PI / 4);
        }
        vel.rotate(M_PI / 6);
    }

    vel = initial_velocaity;
    for (size_t it1 = 0; it1 < 12; ++it1) {
        auto dir = myUnit.direction;
        for (size_t it2 = 0; it2 < 8; ++it2) {
            orders.emplace_back(
                vel * constants.maxUnitForwardSpeed,
                dir,
                model::Aim(false)
            );
            dir.rotate(M_PI / 4);
        }
        vel.rotate(M_PI / 6);
    }

    int min_damage = 1e9;
    model::UnitOrder* best_order;

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

    return *best_order;
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

std::optional<model::UnitOrder> MyStrategy::looting(
        const std::vector<model::Loot>& loots,
        const std::vector<model::Unit*>& enemies,
        const model::Unit& myUnit,
        const model::Zone& zone) {
    if (loots.empty()) {
        return std::nullopt;
    }

    double min_dist = 1e9;
    std::optional<model::Loot> nearest_loot;
    for (auto& loot: loots) {
        if (busy_loot.count(loot.id)) {
            continue;
        }

        if (zone.currentCenter.distToSquared(loot.position) >= zone.currentRadius * zone.currentRadius) {
            continue;
        }

        double min_dist_to_enemy = 1e9;
        for (auto& enemy : enemies) {
            min_dist_to_enemy = std::min(
                min_dist_to_enemy,
                loot.position.distToSquared(enemy->position));
        }

        if (min_dist_to_enemy < constants.viewDistance * constants.viewDistance / 6) {
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
                (constants.weapons[weapon.typeIndex].name == "Bow" && myUnit.ammo[weapon.typeIndex] > 0 && constants.weapons[*myUnit.weapon].name != "Bow") ||
                (zone.currentRadius < radius_treshold && myUnit.ammo[weapon.typeIndex] == 0)) {
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
    busy_loot.insert(nearest_loot->id);

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
