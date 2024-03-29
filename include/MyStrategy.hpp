#ifndef _MY_STRATEGY_HPP_
#define _MY_STRATEGY_HPP_

#include "Simulator.hpp"
#include "DebugInterface.hpp"
#include "model/Constants.hpp"
#include "model/Game.hpp"
#include "model/Order.hpp"
#include "model/Item.hpp"
#include "model/ActionType.hpp"
#include "model/Projectile.hpp"
#include "model/Zone.hpp"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>

using std::cerr;
using std::endl;

#define sqr(x) ((x)*(x))

class MyStrategy {
private:
    static model::Constants* constants_;
public:
    MyStrategy(const model::Constants& constants);
    model::Order getOrder(model::Game& game, DebugInterface* debugInterface);

    std::optional<model::UnitOrder> looting(const model::Unit& myUnit, const model::Zone& zone);
    std::optional<model::UnitOrder> healing(const model::Unit& myUnit) const;

    void shooting(
        const model::Unit& myUnit,
        const model::Unit* nearest_enemy,
        std::vector<const model::Obstacle*>& obstacles,
        std::vector<model::UnitOrder>& orders);

    model::UnitOrder getUnitOrder(model::Unit& myUnit, const model::Zone& zone);

    static model::Constants* getConstants();

    void debugUpdate(int displayedTick, DebugInterface& debugInterface);
    void finish();

    Simulator simulator;
    model::Constants constants;
    std::unordered_map<int, model::Projectile> bullets;
    std::unordered_map<int, model::Unit> enemies;
    std::unordered_map<int, model::Loot> loots;
    std::unordered_map<int, int> busy_loot;
    std::vector<model::Unit*> my_units;

    DebugInterface *debugInterface = nullptr;
    double delta_time;
    double radius_treshold = 100.0;
    int elapsed_time = 0.0;
    model::Vec2 default_dir{1, 0};
};

#endif