#ifndef _SIMULATOR_HPP_
#define _SIMULATOR_HPP_

#include "model/Constants.hpp"
#include "model/Unit.hpp"
#include "model/Projectile.hpp"
#include "model/UnitOrder.hpp"
#include "utility"

class Simulator {
public:
    Simulator(const model::Constants& constants);

    std::pair<model::Vec2, int> SimulateDodging(const model::Unit& unit, std::vector<model::Projectile>& bullets);
    std::pair<model::Vec2, int> Simulate(const model::Unit& unit, std::vector<model::Projectile>& bullets, model::Vec2& target_dir, int ticks);

    int Simulate(model::Unit& unit, model::UnitOrder& order, std::vector<model::Projectile>& bullets, int ticks) const;

    model::Constants constants;
};

#endif