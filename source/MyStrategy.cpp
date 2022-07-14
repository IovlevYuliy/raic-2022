#include "MyStrategy.hpp"
#include <exception>
#include <variant>

model::Constants* MyStrategy::constants_;

model::Constants* MyStrategy::getConstants() {
    return constants_;
}

MyStrategy::MyStrategy(const model::Constants &consts) : constants(consts), simulator(consts) {
    MyStrategy::constants_ = &constants;
}

model::Order MyStrategy::getOrder(model::Game &game, DebugInterface *dbgInterface) {
    debugInterface = dbgInterface;

    std::unordered_map<int, model::UnitOrder> actions;
    std::vector<model::Unit> enemies;
    std::vector<model::Unit> my_units;
    std::vector<model::Projectile> threats;
    for (auto &unit : game.units) {
        if (unit.playerId != game.myId) {
            enemies.push_back(unit);
        }
    }

    for (model::Unit &myUnit : game.units)
    {
        if (myUnit.playerId != game.myId)
            continue;

        myUnit.calcSpeedCircle(constants);

        if (debugInterface) {
            myUnit.showSpeedCircle(dbgInterface);
        }

        std::optional<model::Unit> nearest_enemy;
        double min_dist_to_enemy = 1e9;
        for (auto &enemy : enemies) {
            auto distToEnemy = enemy.position.distToSquared(myUnit.position);
            if(distToEnemy < min_dist_to_enemy) {
                nearest_enemy = enemy;
                min_dist_to_enemy = distToEnemy;
            }
        }

        for (auto &projectile : game.projectiles) {
            if (projectile.shooterPlayerId != game.myId) {
                threats.push_back(projectile);
                if (debugInterface && projectile.intersectUnit(myUnit, constants)) {
                    debugInterface->addPolyLine({projectile.position, projectile.position + projectile.velocity * projectile.lifeTime }, 0.1, debugging::Color(0, 0.3, 0.6, 1));
                }
            }
        }

        // simulateMovement(myUnit, threats);

        auto dodge_dir = dodging(threats, myUnit);

        if (dodge_dir.has_value() && debugInterface) {
            debugInterface->addPolyLine({myUnit.position, (myUnit.position + *dodge_dir) }, 0.1, debugging::Color(1, 0.5, 0, 1));
        }

        bool has_ammo = myUnit.weapon && myUnit.ammo[*myUnit.weapon] > 0;

        if (nearest_enemy && has_ammo && (myUnit.health > 0.2 * constants.unitHealth || nearest_enemy->health < 0.5 * constants.unitHealth) && min_dist_to_enemy < constants.viewDistance * constants.viewDistance / 4) {
            bool shooting = false;
            double aim_delta = 1.0 / constants.weapons[*myUnit.weapon].aimTime / constants.ticksPerSecond;

            model::Projectile bullet(-1, *myUnit.weapon, myUnit.id, game.myId, myUnit.position + myUnit.direction * constants.unitRadius, myUnit.direction * constants.weapons[*myUnit.weapon].projectileSpeed, constants.weapons[*myUnit.weapon].projectileLifeTime);
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

            model::Vec2 move;
            if (dodge_dir.has_value()) {
                move = *dodge_dir;
            } else {
                move = (nearest_enemy->position - myUnit.position).norm();
                if (min_dist_to_enemy < constants.viewDistance * constants.viewDistance / dist_coef) {
                    move.rotate(90).mul(constants.maxUnitForwardSpeed / 2);
                } else {
                    move.mul(constants.maxUnitForwardSpeed);
                }
            }
            model::UnitOrder order(
                move,
                (nearest_enemy->position + nearest_enemy->velocity * time_to_hit) - myUnit.position,
                model::Aim(shooting)
            );

            if (debugInterface && shooting) {
                debugInterface->addPolyLine({myUnit.position, (nearest_enemy->position + nearest_enemy->velocity * 0.5) }, 0.1, debugging::Color(0, 0, 1, 1));
            }

            actions.insert({myUnit.id, order});

            // cerr << game.currentTick << ' ' << std::noboolalpha << shooting << endl;
            continue;
        }


        auto healing_order = healing(myUnit);
        auto loot_order = looting(game.loot, myUnit, game.zone);

        if (healing_order) {
            if (dodge_dir) {
                healing_order->targetVelocity = *dodge_dir;
            }

            if (loot_order && !dodge_dir) {
                healing_order->targetDirection = loot_order->targetDirection;
                healing_order->targetVelocity = loot_order->targetVelocity;
            }
            actions.insert({myUnit.id, *healing_order});
            continue;
        }

        if (dodge_dir) {
            model::UnitOrder order(
                *dodge_dir,
                myUnit.direction,
                std::nullopt);
            actions.insert({myUnit.id, order});
            continue;
        }

        if (loot_order) {
            actions.insert({myUnit.id, *loot_order});
            continue;
        }

        if (nearest_enemy) {
            model::UnitOrder order(
                (nearest_enemy->position - myUnit.position).norm().mul(constants.maxUnitForwardSpeed / 2),
                (nearest_enemy->position - myUnit.position),
                std::nullopt);
            actions.insert({myUnit.id, order});
            continue;
        }

        model::UnitOrder order(
            (game.zone.nextCenter - myUnit.position) * constants.zoneSpeed,
            {-myUnit.direction.y, myUnit.direction.x},
            std::nullopt);
        actions.insert({myUnit.id, order});
    }

    return model::Order(actions);
}

void MyStrategy::simulateMovement(const model::Unit& myUnit, const std::vector<model::Projectile>& bullets) const {
    double offset = 360.0 / 16.0;
    for (int ang = 0; ang < 360; ang += offset) {
        auto dir = myUnit.direction.clone().rotate(ang);

        auto sim_unit(myUnit);
        auto sim_bullets(bullets);
        model::UnitOrder order(
            dir * constants.maxUnitForwardSpeed,
            sim_unit.direction,
            std::nullopt);

        auto damage = simulator.Simulate(sim_unit, order, sim_bullets, 30);

        if (debugInterface) {
            debugInterface->addRing(sim_unit.position, constants.unitRadius, 0.1, debugging::Color(0, 0, 1, 1));
            debugInterface->addPlacedText(sim_unit.position, std::to_string(damage), {0, -1}, 0.3, debugging::Color(0, 0, 0, 0.5));
        }
    }
}

std::optional<model::Vec2> MyStrategy::dodging(std::vector<model::Projectile>& bullets, const model::Unit& myUnit) {
    if (bullets.empty()) {
        return std::nullopt;
    }

    double offset = 360.0 / 16.0;
    double min_damage = 1e9;
    std::optional<model::Vec2> res;

    for (int ang = 0; ang < 360; ang += offset) {
        auto dir = myUnit.direction.clone().rotate(ang);

        auto sim_unit(myUnit);
        auto sim_bullets(bullets);
        model::UnitOrder order(
            dir * constants.maxUnitForwardSpeed,
            sim_unit.direction,
            std::nullopt);

        auto damage = simulator.Simulate(sim_unit, order, sim_bullets, 30);
        if (damage < min_damage) {
            min_damage = damage;
            res = dir * constants.maxUnitForwardSpeed;
        }

        if (debugInterface) {
            debugInterface->addRing(sim_unit.position, constants.unitRadius, 0.1, debugging::Color(0, 0, 1, 1));
            debugInterface->addPlacedText(sim_unit.position, std::to_string(damage), {0, -1}, 0.3, debugging::Color(0, 0, 0, 0.5));
        }
    }

    return res;
}

std::optional<model::UnitOrder> MyStrategy::healing(const model::Unit& myUnit) const {
    if (constants.maxShield - myUnit.shield >= constants.shieldPerPotion && myUnit.shieldPotions > 0) {
        return model::UnitOrder(
            myUnit.direction * constants.maxUnitForwardSpeed,
            {-myUnit.position.x, -myUnit.position.y},
            model::UseShieldPotion()
        );
    }

    return std::nullopt;
}

std::optional<model::UnitOrder> MyStrategy::looting(std::vector<model::Loot>& loots, const model::Unit& myUnit, const model::Zone& zone) const {
    if (loots.empty()) {
        return std::nullopt;
    }

    double min_dist = 1e9;
    std::optional<model::Loot> nearest_loot;
    for (auto& loot: loots) {
        double dist_to_loot = loot.position.distToSquared(myUnit.position);
        if (dist_to_loot > min_dist) {
            continue;
        }

        if (std::holds_alternative<model::ShieldPotions>(loot.item) && myUnit.shieldPotions < constants.maxShieldPotionsInInventory / 2) {
            nearest_loot = loot;
            min_dist = dist_to_loot;
        }

        if (std::holds_alternative<model::Ammo>(loot.item) && myUnit.health > constants.unitHealth * 0.7) {
            auto ammo = std::get<model::Ammo>(loot.item);
            if (constants.weapons[ammo.weaponTypeIndex].name == "Magic wand") {
                continue;
            }

            if (myUnit.ammo[ammo.weaponTypeIndex] < 0.9 * constants.weapons[ammo.weaponTypeIndex].maxInventoryAmmo) {
                nearest_loot = loot;
                min_dist = dist_to_loot;
            }
        }

        if (std::holds_alternative<model::Weapon>(loot.item) && myUnit.health > constants.unitHealth * 0.7) {
            auto weapon = std::get<model::Weapon>(loot.item);
            if (constants.weapons[weapon.typeIndex].name == "Magic wand") {
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

    if (!nearest_loot || zone.currentCenter.distToSquared(nearest_loot->position) >= zone.currentRadius * zone.currentRadius) {
        return std::nullopt;
    }

    auto dir = (*nearest_loot).position - myUnit.position;
    dir.norm();

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

void MyStrategy::finish() {}
