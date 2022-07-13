#ifndef _MY_STRATEGY_HPP_
#define _MY_STRATEGY_HPP_

#include "Simulator.hpp"
#include "DebugInterface.hpp"
#include "model/Constants.hpp"
#include "model/Game.hpp"
#include "model/Order.hpp"
#include "model/Item.hpp"
#include "model/ActionType.hpp"
#include "iostream"
#include "memory"

using std::cerr;
using std::endl;

class MyStrategy {
private:
    static model::Constants* constants_;
public:
    MyStrategy(const model::Constants& constants);
    model::Order getOrder(model::Game& game, DebugInterface* debugInterface);

    model::Vec2 dodging(std::vector<model::Projectile>& bullets, const model::Unit& myUnit);
    std::optional<model::UnitOrder> looting(std::vector<model::Loot>& loots, const model::Unit& myUnit, const model::Zone& zone) const;
    std::optional<model::UnitOrder> healing(const model::Unit& myUnit) const;

    static model::Constants* getConstants();

    void debugUpdate(int displayedTick, DebugInterface& debugInterface);
    void finish();

    Simulator simulator;
    model::Constants constants;
    DebugInterface *debugInterface = nullptr;
};

#endif