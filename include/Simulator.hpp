#ifndef _SIMULATOR_HPP_
#define _SIMULATOR_HPP_

#include "model/Constants.hpp"
#include "model/Unit.hpp"
#include "model/Projectile.hpp"
#include "model/UnitOrder.hpp"
#include "model/Zone.hpp"
#include "utility"

class Simulator {
public:
    Simulator(const model::Constants& constants);

    std::optional<const model::Obstacle*> SimulateMovement(
        model::Unit& unit,
        model::UnitOrder& order,
        const std::vector<const model::Obstacle*>& obstacles,
        int cur_tick);

    int Simulate(
        model::Unit& unit,
        model::UnitOrder& order,
        std::vector<model::Projectile>& bullets,
        const std::vector<const model::Obstacle*>& obstacles,
        const model::Zone& zone,
        int ticks) const;

    model::Constants constants;
    int started_tick = 0;
    double delta_time;
};

#endif