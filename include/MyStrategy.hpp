#ifndef _MY_STRATEGY_HPP_
#define _MY_STRATEGY_HPP_

#include "Simulator.hpp"
#include "DebugInterface.hpp"
#include "model/Constants.hpp"
#include "model/Game.hpp"
#include "model/Order.hpp"

class MyStrategy {
public:
    MyStrategy(const model::Constants& constants);
    model::Order getOrder(model::Game& game, DebugInterface* debugInterface);
    model::Vec2 dodging(std::vector<model::Projectile>& bullets, const model::Unit& myUnit);

    void debugUpdate(DebugInterface& debugInterface);
    void finish();

    Simulator simulator;
    model::Constants constants;
    DebugInterface *debugInterface = nullptr;
};

#endif