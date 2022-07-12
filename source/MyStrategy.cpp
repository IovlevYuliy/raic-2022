#include "MyStrategy.hpp"
#include <exception>

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
        double min_dist = 1e9;
        for (auto &enemy : enemies) {
            auto distToEnemy = enemy.position.distToSquared(myUnit.position);
            if(distToEnemy < min_dist) {
                nearest_enemy = enemy;
                min_dist = distToEnemy;
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

        auto direction = dodging(threats, myUnit);
        bool has_ammo = myUnit.weapon && myUnit.ammo[*myUnit.weapon] > 0;

        if (nearest_enemy && has_ammo && myUnit.health > 0.5 * constants.unitHealth && min_dist < constants.viewDistance * constants.viewDistance / 4) {
            bool shooting = false;

            if (fabs(1.0 - myUnit.aim) < 1e-6) {
                model::Projectile bullet(-1, *myUnit.weapon, myUnit.id, game.myId, myUnit.position + myUnit.direction * constants.unitRadius, myUnit.direction * constants.weapons[*myUnit.weapon].projectileSpeed, constants.weapons[*myUnit.weapon].projectileLifeTime);

                double min_dist = 1e9;
                std::optional<model::Obstacle> nearest_obst;

                for (auto& obstacle: constants.obstacles) {
                    if (obstacle.canShootThrough) continue;
                    if (!bullet.intersectCircle(obstacle.position, obstacle.radius)) continue;
                    double dist_to_obst = obstacle.position.distTo(bullet.position) - obstacle.radius;
                    if (dist_to_obst < min_dist) {
                        nearest_obst = obstacle;
                        min_dist = dist_to_obst;
                    }
                }

                double dist_to_enemy = bullet.position.distTo(nearest_enemy->position) - constants.unitRadius;
                shooting = bullet.intersectUnit(*nearest_enemy, constants) && (!nearest_obst || dist_to_enemy <=  min_dist);
            }

            auto aim = std::make_shared<model::ActionOrder::Aim>(shooting);
            auto action = std::make_optional(aim);
            model::UnitOrder order(
                direction,
                nearest_enemy->position - myUnit.position,
                action
            );
            actions.insert({myUnit.id, order});
            continue;
        }

        auto healing_order = healing(myUnit);
        auto loot_order = looting(game.loot, myUnit, game.zone);

        if (healing_order) {
            if (loot_order) {
                healing_order->targetDirection = loot_order->targetDirection;
                healing_order->targetVelocity = loot_order->targetVelocity;
            }
            actions.insert({myUnit.id, *healing_order});
            continue;
        }

        if (loot_order) {
            actions.insert({myUnit.id, *loot_order});
            continue;
        }

        if (!nearest_enemy && threats.empty()) {
            direction = (game.zone.nextCenter - myUnit.position) * constants.zoneSpeed;
        }

        if (nearest_enemy) {
            direction = (nearest_enemy->position - myUnit.position).norm().mul(constants.maxUnitForwardSpeed / 2);
        }

        model::UnitOrder order(
            direction,
            {-myUnit.direction.y, myUnit.direction.x},
            std::nullopt);
        actions.insert({myUnit.id, order});
    }

    return model::Order(actions);
}

model::Vec2 MyStrategy::dodging(std::vector<model::Projectile>& bullets, const model::Unit& myUnit) {
    if (bullets.empty()) {
        return myUnit.velocity;
    }

    auto res = simulator.Simulate(myUnit, bullets, 1);
    std::cerr << res.second << std::endl;
    return res.first;
}

std::optional<model::UnitOrder> MyStrategy::healing(const model::Unit& myUnit) const {
    if (constants.maxShield - myUnit.shield >= constants.shieldPerPotion && myUnit.shieldPotions > 0) {
        return model::UnitOrder(
            myUnit.direction * constants.maxUnitForwardSpeed,
            {-myUnit.position.x, -myUnit.position.y},
            std::make_shared<model::ActionOrder::UseShieldPotion>()
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
        if (!loot.item) {
            continue;
        }

        double dist_to_loot = loot.position.distToSquared(myUnit.position);
        if (dist_to_loot > min_dist) {
            continue;
        }

        if (loot.item->getTag() == model::Item::ShieldPotions::TAG && myUnit.shieldPotions < constants.maxShieldPotionsInInventory / 2) {
            nearest_loot = loot;
            min_dist = dist_to_loot;
        }

        if (loot.item->getTag() == model::Item::Ammo::TAG && myUnit.health > constants.unitHealth * 0.7) {
            auto ammo = std::dynamic_pointer_cast<model::Item::Ammo>(loot.item);
            if (constants.weapons[ammo->weaponTypeIndex].name == "Magic wand") {
                continue;
            }

            if (myUnit.ammo[ammo->weaponTypeIndex] < 0.9 * constants.weapons[ammo->weaponTypeIndex].maxInventoryAmmo) {
                nearest_loot = loot;
                min_dist = dist_to_loot;
            }
        }

        if (loot.item->getTag() == model::Item::Weapon::TAG && myUnit.health > constants.unitHealth * 0.7) {
            auto weapon = std::dynamic_pointer_cast<model::Item::Weapon>(loot.item);
            if (constants.weapons[weapon->typeIndex].name == "Magic wand") {
                continue;
            }

            if (!myUnit.weapon || (myUnit.ammo[weapon->typeIndex] > 0 && constants.weapons[*myUnit.weapon].name == "Magic wand") || (constants.weapons[weapon->typeIndex].name == "Bow" && myUnit.ammo[weapon->typeIndex] > 0 && constants.weapons[*myUnit.weapon].name != "Bow")) {
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
        std::make_shared<model::ActionOrder::Pickup>(((*nearest_loot).id))
    );
}

void MyStrategy::debugUpdate(DebugInterface& dbgInterface) {}

void MyStrategy::finish() {}
