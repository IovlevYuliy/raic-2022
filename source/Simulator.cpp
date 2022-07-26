#include "Simulator.hpp"
#include "iostream"
#include <algorithm>
#include <variant>

const int SIMULATED_TICKS = 30;
Simulator::Simulator(const model::Constants &constants) : constants(constants) {}

std::pair<model::Vec2, int> Simulator::Simulate(const model::Unit& unit, std::vector<model::Projectile>& bullets, model::Vec2& target_dir, int ticks) {
    if (!ticks)  {
        return {{0, 0}, 1000};
    }

    int damage = 0;
    auto max_velocity = unit.getVelocity(target_dir).len();
    auto cur_len = unit.velocity.len();
    auto cur_velocity = target_dir * cur_len;

    for (auto& bullet : bullets) {
        if (bullet.intersectUnit(unit, constants)) {
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

int Simulator::Simulate(
        model::Unit& unit, model::UnitOrder& order,
        std::vector<model::Projectile>& bullets,
        const std::vector<const model::Obstacle*>& obstacles,
        const model::Zone& zone,
        int cur_tick) const {
    if (cur_tick - started_tick >= SIMULATED_TICKS)  {
        return 0;
    }

    int damage = 0;

    // SIMULATE UNIT AIM
    if (unit.weapon && order.action && std::holds_alternative<model::Aim>(*order.action)) {
        unit.aim += delta_time / constants.weapons[*unit.weapon].aimTime;
    } else {
        unit.aim -= delta_time / constants.weapons[*unit.weapon].aimTime;
    }
    unit.aim = std::clamp(unit.aim, 0.0, 1.0);

    // SIMULATE UNIT ROTATION
    double diff_angle = atan2(order.targetDirection.cross(unit.direction), order.targetDirection.dot(unit.direction)) * 180 / M_PI;
    double aim_rotation_speed = unit.weapon ? constants.weapons[*unit.weapon].aimRotationSpeed : 0;
    double rotation_speed = constants.rotationSpeed - (constants.rotationSpeed - aim_rotation_speed) * unit.aim;
    double sign = diff_angle > 0 ? -1 : 1;

    double angle_shift = sign * std::min(fabs(diff_angle), rotation_speed * delta_time) * M_PI / 180;
    unit.direction.rotate(angle_shift);

    // SIMULATE UNIT SHOOTING
    if (std::holds_alternative<model::Aim>(*order.action) && std::get<model::Aim>(*order.action).shoot && 1.0 - unit.aim < 1e-6 && unit.nextShotTick <= cur_tick) {
        // bool has_hit = false;
        // auto bullet_ray = model::Ray(unit.position, unit.direction);
        // for (auto& enemy : enemies) {
        //     has_hit = has_hit || bullet_ray.intersectsCircle(enemy.position, constants.unitRadius);
        // }
        // if (has_hit) {
        unit.nextShotTick = 1e9;
        //cur_tick + ceil(1 / constants.weapons[*unit.weapon].roundsPerSecond * constants.ticksPerSecond);
        damage -= constants.weapons[*unit.weapon].projectileDamage / 2;
        // }
    }

    // SIMULATE UNIT MOVEMENT
    unit.calcSpeedCircle(constants);
    auto move_dir = order.targetVelocity.clone().norm();
    auto max_velocity_len = unit.getVelocity(move_dir).len();
    model::Vec2 target_velocity;
    if (max_velocity_len >= order.targetVelocity.len()) {
        target_velocity = order.targetVelocity;
    } else {
        target_velocity = move_dir.mul(max_velocity_len);
    }

    auto velocity_shift = (target_velocity - unit.velocity);
    if (velocity_shift.len() > constants.unitAcceleration * delta_time) {
        velocity_shift.norm().mul(constants.unitAcceleration * delta_time);
    }
    unit.velocity += velocity_shift;

    bool has_collision = false;
    for (auto& obstacle : obstacles) {
        auto hit = unit.hasHit(*obstacle);

        if (hit) {
            has_collision = true;
            double f1_time = *hit;
            double f2_time = delta_time - f1_time;
            unit.next_position = unit.position + unit.velocity * f1_time;
            auto v = (obstacle->position - unit.next_position).norm();
            unit.velocity = model::Vec2(v.y, -v.x) * (v.cross(unit.velocity) / unit.velocity.len());
            unit.next_position += unit.velocity * f2_time;
            break;
        }
    }

    if (!has_collision) {
        unit.next_position = unit.position + unit.velocity * delta_time;
    }

    // SIMULATE BULLETS MOVEMENT
    for (auto& bullet : bullets) {
        if (bullet.destroyed) {
            continue;
        }

        std::optional<model::Vec2> obstacle_hit;
        double obstacle_min_dist = 1e9;
        for (auto& obstacle : obstacles) {
            if (obstacle->canShootThrough) {
                continue;
            }
            if ((bullet.position.distTo(obstacle->position) - obstacle->radius) / constants.weapons[bullet.weaponTypeIndex].projectileSpeed > delta_time) {
                continue;
            }
            auto hit = bullet.hasHit(*obstacle);
            if (!hit) {
                continue;
            }
            double dist = bullet.position.distTo(*hit);
            if (dist < obstacle_min_dist) {
                obstacle_min_dist = dist;
                obstacle_hit = hit;
            }
        }

        auto unit_hit = bullet.hasHit(unit);

        if (!unit_hit && obstacle_hit) {
            bullet.destroyed = true;
            continue;
        }

        if (unit_hit && !obstacle_hit) {
            // std::cerr << "HAS HIT! +" << constants.weapons[bullet.weaponTypeIndex].projectileDamage << " damage\n";
            damage += constants.weapons[bullet.weaponTypeIndex].projectileDamage;
            bullet.destroyed = true;
            continue;
        }

        if (unit_hit && obstacle_hit) {
            // std::cerr << "HAS HIT! +" << constants.weapons[bullet.weaponTypeIndex].projectileDamage << " damage\n";
            if (obstacle_min_dist > bullet.position.distTo(unit.position) - constants.unitRadius) {
                damage += constants.weapons[bullet.weaponTypeIndex].projectileDamage;
            }
            bullet.destroyed = true;
            continue;
        }

        if (bullet.lifeTime <= delta_time) {
            bullet.destroyed = true;
            continue;
        }

        bullet.position += bullet.velocity * delta_time;
        bullet.lifeTime -= delta_time;
    }

    unit.position = unit.next_position;

    if (zone.currentCenter.distTo(unit.position) + constants.unitRadius >= zone.currentRadius) {
        damage += 2;
    }

    damage += Simulate(unit, order, bullets, obstacles, zone, cur_tick + 1);

    return damage;
}
