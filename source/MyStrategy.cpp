#include "MyStrategy.hpp"
#include <exception>
#include <iostream>

MyStrategy::MyStrategy(const model::Constants &constants) : constants(constants), simulator(constants) {}

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
            }
        }

        auto direction = myUnit.velocity; // dodging(threats, myUnit);

        if (nearest_enemy && min_dist < constants.viewDistance * constants.viewDistance / 4) {
            auto aim = std::make_shared<model::ActionOrder::Aim>(true);
            auto action = std::make_optional(aim);
            model::UnitOrder order(
                (*nearest_enemy).position - myUnit.position,
                (*nearest_enemy).position - myUnit.position,
                action
            );
            actions.insert({myUnit.id, order});
            continue;
        }

        if (threats.empty()) {
            direction = (game.zone.nextCenter - myUnit.position) * constants.zoneSpeed;
        }

        model::UnitOrder order(
            direction,
            {-myUnit.direction.y, myUnit.direction.x},
            std::nullopt);
        actions.insert({myUnit.id, order});
    }

    if (debugInterface) {
        for (auto const& [key, val] : actions) {
            model::Vec2 pos;
            for (auto &unit : game.units) {
                if (unit.id == key) {
                    pos = unit.position;
                    break;
                }
            }

            // debugInterface->addPolyLine({pos, pos + val.targetVelocity}, 0.1, debugging::Color(0, 1, 0, 1));
            // debugInterface->addPolyLine({pos, pos + val.targetDirection}, 0.1, debugging::Color(1, 0, 0, 1));

            for (auto const& threat: threats) {
                // debugInterface->addPolyLine({pos, threat.position}, 0.1, debugging::Color(0, 0, 0, 1));
            }
        }
    }

    return model::Order(actions);
}

model::Vec2 MyStrategy::dodging(std::vector<model::Projectile>& bullets, const model::Unit& myUnit) {
    if (bullets.empty()) {
        return myUnit.velocity;
    }

    double max_time = 1e9;
    for (auto &bullet : bullets) {
        double t_time = bullet.position.distTo(myUnit.position) / constants.weapons[bullet.weaponTypeIndex].projectileSpeed;
        max_time = std::min(t_time, max_time);
    }

    auto get_damage = [&bullets, &myUnit, this](const model::Vec2& position) -> double {
        double sum_damage = 0;
        for (auto &bullet : bullets) {
            if (bullet.intersectCircle(position, this->constants.unitRadius)) {
                sum_damage += this->constants.weapons[bullet.weaponTypeIndex].projectileDamage;
            }
        }

        return sum_damage;
    };

    auto res = myUnit.velocity;
    auto dir = myUnit.velocity;
    double min_damage = 1e9;
    for (int i = 0; i < 360; ++i) {
        dir.rotate(1);
        for (double move_time = 0; move_time <= max_time; move_time += (max_time / 1000)) {
            model::Vec2 new_position = myUnit.position + dir * move_time;
            double damage = get_damage(new_position);
            if (damage < min_damage) {
                min_damage = damage;
                res = dir;
            }
        }
    }
    std::cerr << "Min damage " << min_damage << std::endl;

    return res;
}

// void MyStrategy::simulateMovement() {}

void MyStrategy::debugUpdate(DebugInterface& dbgInterface) {}

void MyStrategy::finish() {}
