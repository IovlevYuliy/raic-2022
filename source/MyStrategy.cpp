#include "MyStrategy.hpp"
#include <exception>
#include <iostream>

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
            if(distToEnemy < min_dist && (!enemy.weapon || constants.weapons[*enemy.weapon].name != "Bow")) {
                nearest_enemy = enemy;
                min_dist = distToEnemy;
            }
        }

        for (auto &projectile : game.projectiles) {
            if (projectile.shooterPlayerId != game.myId) {
                threats.push_back(projectile);
                if (debugInterface) {
                    debugInterface->addPolyLine({projectile.position, projectile.position + projectile.velocity * projectile.lifeTime }, 0.1, debugging::Color(0, 0.3, 0.6, 1));
                }
            }
        }

        auto direction = dodging(threats, myUnit);

        if (nearest_enemy && min_dist < constants.viewDistance * constants.viewDistance / 4) {
            auto aim = std::make_shared<model::ActionOrder::Aim>(true);
            auto action = std::make_optional(aim);
            model::UnitOrder order(
                direction,
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

    return model::Order(actions);
}

model::Vec2 MyStrategy::dodging(std::vector<model::Projectile>& bullets, const model::Unit& myUnit) {
    // return myUnit.velocity;

    if (bullets.empty()) {
        return myUnit.velocity;
    }

    auto res = simulator.Simulate(myUnit, bullets, 1);
    std::cerr << res.second << std::endl;
    return res.first;
}

// void MyStrategy::simulateMovement() {}

void MyStrategy::debugUpdate(DebugInterface& dbgInterface) {}

void MyStrategy::finish() {}
