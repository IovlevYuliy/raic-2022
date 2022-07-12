#ifndef _SIMULATOR_HPP_
#define _SIMULATOR_HPP_

#include "model/Constants.hpp"
#include "model/Unit.hpp"
#include "model/Projectile.hpp"
#include "utility"

class Simulator {
public:
    Simulator(const model::Constants& constants);

    std::pair<model::Vec2, int> Simulate(const model::Unit& unit, std::vector<model::Projectile>& bullets, int ticks);

    model::Constants constants;
};

#endif