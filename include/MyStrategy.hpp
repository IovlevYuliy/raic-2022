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

using std::cerr;
using std::endl;

class MyStrategy {
private:
    static model::Constants* constants_;
public:
    MyStrategy(const model::Constants& constants);
    model::Order getOrder(model::Game& game, DebugInterface* debugInterface);

    std::optional<model::Vec2> dodging(std::vector<model::Projectile>& bullets, const model::Unit& myUnit);
    std::optional<model::UnitOrder> looting(const std::vector<model::Loot>& loots, const model::Unit& myUnit, const model::Zone& zone) const;
    std::optional<model::UnitOrder> healing(const model::Unit& myUnit) const;

    void simulateMovement(const model::Unit& myUnit, const std::vector<model::Projectile>& bullets) const;
    model::UnitOrder getUnitOrder(model::Unit& myUnit, const std::vector<model::Unit>& enemies, const std::vector<model::Loot>& loot, const model::Zone& zone) const;

    static model::Constants* getConstants();

    void debugUpdate(int displayedTick, DebugInterface& debugInterface);
    void finish();

    Simulator simulator;
    model::Constants constants;
    std::unordered_map<int, model::Projectile> bullets;
    DebugInterface *debugInterface = nullptr;
    double delta_time;
    double radius_treshold = 40.0;
};

#endif