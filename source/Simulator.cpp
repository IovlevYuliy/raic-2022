#include "Simulator.hpp"
#include "iostream"

Simulator::Simulator(const model::Constants &constants) : constants(constants) {}

std::pair<model::Vec2, int> Simulator::SimulateDodging(const model::Unit& unit, std::vector<model::Projectile>& bullets) {
    auto dir = unit.direction;
    double delta_time = 1 / constants.ticksPerSecond;

    int min_damage = 1e9;
    model::Vec2 res_dir;

    for (int ang = 0; ang < 360; ++ang) {
        dir.rotate(ang);

        auto res = Simulate(unit, bullets, dir, 10);
        if (res.second < min_damage) {
            // std::cerr << "damage " << res.second << " " << "angle " << ang << std::endl;
            min_damage = res.second;
            res_dir = res.first;
        }
    }

    return { res_dir, min_damage };
}

std::pair<model::Vec2, int> Simulator::Simulate(const model::Unit& unit, std::vector<model::Projectile>& bullets, model::Vec2& target_dir, int ticks) {
    if (!ticks)  {
        return {{0, 0}, 1000};
    }

    int damage = 0;
    double delta_time = 1.0 / constants.ticksPerSecond;
    auto max_velocity = unit.getVelocity(target_dir).len();
    auto cur_len = unit.velocity.len();
    auto cur_velocity = target_dir * cur_len;

    for (auto& bullet : bullets) {
        // std::cerr << bullet.hasHit(unit, velocity) << std::endl;
        if (bullet.intersectUnit(unit, constants)) {
            // std::cerr << "HAS HIT! +" << constants.weapons[bullet.weaponTypeIndex].projectileDamage << " damage\n";
            damage += constants.weapons[bullet.weaponTypeIndex].projectileDamage;
        }
    }

    if (!damage) {
        return { cur_velocity, damage };
    }

    model::Unit sim_unit(unit);
    sim_unit.position += cur_velocity * delta_time;
    auto res = Simulate(sim_unit, bullets, target_dir, ticks - 1);

    return { cur_velocity, std::min(damage, res.second) };
}

int Simulator::Simulate(model::Unit& unit, model::UnitOrder& order, std::vector<model::Projectile>& bullets, int ticks) const {
    if (!ticks)  {
        return 0;
    }

    int damage = 0;
    double delta_time = 1.0 / constants.ticksPerSecond;

    auto move_dir = order.targetVelocity.clone().norm();
    auto max_velocity_len = unit.getVelocity(move_dir).len();
    model::Vec2 target_velocity;
    if (max_velocity_len > order.targetVelocity.len()) {
        target_velocity = order.targetVelocity;
    } else {
        target_velocity = move_dir.mul(max_velocity_len);
    }

    auto velocity_shift = (target_velocity - unit.velocity).norm().mul(constants.unitAcceleration * delta_time);

    unit.velocity += velocity_shift;

    for (auto& bullet : bullets) {
        if (!bullet.destroyed && bullet.hasHit(unit)) {
            // std::cerr << "HAS HIT! +" << constants.weapons[bullet.weaponTypeIndex].projectileDamage << " damage\n";
            damage += constants.weapons[bullet.weaponTypeIndex].projectileDamage;
            bullet.destroyed = true;
            continue;
        }

        if (bullet.lifeTime < delta_time) {
            bullet.destroyed = true;
            continue;
        }

        bullet.position += bullet.velocity * delta_time;
        bullet.lifeTime -= delta_time;
    }

    unit.position += unit.velocity * delta_time;

    // if (!damage) {
    //     return { cur_velocity, damage };
    // }

    // model::Unit sim_unit(unit);
    // sim_unit.position += cur_velocity * delta_time;
    // auto res = Simulate(sim_unit, bullets, target_dir, ticks - 1);

    damage += Simulate(unit, order, bullets, ticks - 1);

    return damage;
}
