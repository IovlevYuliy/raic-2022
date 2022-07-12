#include "Simulator.hpp"
#include "iostream"

Simulator::Simulator(const model::Constants &constants) : constants(constants) {}

std::pair<model::Vec2, int> Simulator::Simulate(const model::Unit& unit, std::vector<model::Projectile>& bullets, int ticks) {
    if (!ticks) {
        return {{0, 0}, 0};
    }

    auto dir = unit.direction;
    double delta_time = 1 / constants.ticksPerSecond;
    int min_damage = 1e9;
    model::Vec2 res_dir;
    for (int ang = 0; ang < 360; ++ang) {
        dir.rotate(ang);
        auto velocity = unit.getVelocity(dir);
        int damage = 0;

        std::vector<model::Projectile>& sim_bullets = bullets;
        std::vector<model::Projectile> rest_bullets;
        for (auto& bullet : sim_bullets) {
            // std::cerr << bullet.hasHit(unit, velocity) << std::endl;
            if (!bullet.destroyed && bullet.hasHit(unit, velocity)) {
                damage += constants.weapons[bullet.weaponTypeIndex].projectileDamage;
                bullet.destroyed = true;
                continue;
            }

            if (bullet.lifeTime <= delta_time) {
                bullet.destroyed = true;
                continue;
            }


            bullet.position += bullet.velocity * delta_time;
            bullet.lifeTime -= delta_time;
            rest_bullets.push_back(bullet);
        }

        model::Unit sim_unit(unit);
        sim_unit.position += velocity * delta_time;
        auto res = Simulate(sim_unit, rest_bullets, ticks - 1);

        if (damage + res.second < min_damage) {
            min_damage = damage;
            res_dir = velocity;
        }
    }

    return { res_dir, min_damage };
}