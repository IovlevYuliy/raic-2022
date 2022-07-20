#include "Unit.hpp"
#include <iostream>
#include <algorithm>
#include "MyStrategy.hpp"

namespace model {

Unit::Unit(int id, int playerId, double health, double shield, int extraLives, model::Vec2 position, std::optional<double> remainingSpawnTime, model::Vec2 velocity, model::Vec2 direction, double aim, std::optional<model::Action> action, int healthRegenerationStartTick, std::optional<int> weapon, int nextShotTick, std::vector<int> ammo, int shieldPotions) : id(id), playerId(playerId), health(health), shield(shield), extraLives(extraLives), position(position), remainingSpawnTime(remainingSpawnTime), velocity(velocity), direction(direction), aim(aim), action(action), healthRegenerationStartTick(healthRegenerationStartTick), weapon(weapon), nextShotTick(nextShotTick), ammo(ammo), shieldPotions(shieldPotions) { }
    // Read Unit from input stream
    Unit Unit::readFrom(InputStream& stream) {
        int id = stream.readInt();
        int playerId = stream.readInt();
        double health = stream.readDouble();
        double shield = stream.readDouble();
        int extraLives = stream.readInt();
        model::Vec2 position = model::Vec2::readFrom(stream);
        std::optional<double> remainingSpawnTime = std::optional<double>();
        if (stream.readBool()) {
            double remainingSpawnTimeValue = stream.readDouble();
            remainingSpawnTime.emplace(remainingSpawnTimeValue);
        }
        model::Vec2 velocity = model::Vec2::readFrom(stream);
        model::Vec2 direction = model::Vec2::readFrom(stream);
        double aim = stream.readDouble();
        std::optional<model::Action> action = std::optional<model::Action>();
        if (stream.readBool()) {
            model::Action actionValue = model::Action::readFrom(stream);
            action.emplace(actionValue);
        }
        int healthRegenerationStartTick = stream.readInt();
        std::optional<int> weapon = std::optional<int>();
        if (stream.readBool()) {
            int weaponValue = stream.readInt();
            weapon.emplace(weaponValue);
        }
        int nextShotTick = stream.readInt();
        std::vector<int> ammo = std::vector<int>();
        size_t ammoSize = stream.readInt();
        ammo.reserve(ammoSize);
        for (size_t ammoIndex = 0; ammoIndex < ammoSize; ammoIndex++) {
            int ammoElement = stream.readInt();
            ammo.emplace_back(ammoElement);
        }
        int shieldPotions = stream.readInt();
        return Unit(id, playerId, health, shield, extraLives, position, remainingSpawnTime, velocity, direction, aim, action, healthRegenerationStartTick, weapon, nextShotTick, ammo, shieldPotions);
    }

    // Write Unit to output stream
    void Unit::writeTo(OutputStream& stream) const {
        stream.write(id);
        stream.write(playerId);
        stream.write(health);
        stream.write(shield);
        stream.write(extraLives);
        position.writeTo(stream);
        if (remainingSpawnTime) {
            stream.write(true);
            const double& remainingSpawnTimeValue = *remainingSpawnTime;
            stream.write(remainingSpawnTimeValue);
        } else {
            stream.write(false);
        }
        velocity.writeTo(stream);
        direction.writeTo(stream);
        stream.write(aim);
        if (action) {
            stream.write(true);
            const model::Action& actionValue = *action;
            actionValue.writeTo(stream);
        } else {
            stream.write(false);
        }
        stream.write(healthRegenerationStartTick);
        if (weapon) {
            stream.write(true);
            const int& weaponValue = *weapon;
            stream.write(weaponValue);
        } else {
            stream.write(false);
        }
        stream.write(nextShotTick);
        stream.write((int)(ammo.size()));
        for (const int& ammoElement : ammo) {
            stream.write(ammoElement);
        }
        stream.write(shieldPotions);
    }

    // Get string representation of Unit
    std::string Unit::toString() const {
        std::stringstream ss;
        ss << "Unit { ";
        ss << "id: ";
        ss << id;
        ss << ", ";
        ss << "playerId: ";
        ss << playerId;
        ss << ", ";
        ss << "health: ";
        ss << health;
        ss << ", ";
        ss << "shield: ";
        ss << shield;
        ss << ", ";
        ss << "extraLives: ";
        ss << extraLives;
        ss << ", ";
        ss << "position: ";
        ss << position.toString();
        ss << ", ";
        ss << "remainingSpawnTime: ";
        if (remainingSpawnTime) {
            const double& remainingSpawnTimeValue = *remainingSpawnTime;
            ss << remainingSpawnTimeValue;
        } else {
            ss << "none";
        }
        ss << ", ";
        ss << "velocity: ";
        ss << velocity.toString();
        ss << ", ";
        ss << "direction: ";
        ss << direction.toString();
        ss << ", ";
        ss << "aim: ";
        ss << aim;
        ss << ", ";
        ss << "action: ";
        if (action) {
            const model::Action& actionValue = *action;
            ss << actionValue.toString();
        } else {
            ss << "none";
        }
        ss << ", ";
        ss << "healthRegenerationStartTick: ";
        ss << healthRegenerationStartTick;
        ss << ", ";
        ss << "weapon: ";
        if (weapon) {
            const int& weaponValue = *weapon;
            ss << weaponValue;
        } else {
            ss << "none";
        }
        ss << ", ";
        ss << "nextShotTick: ";
        ss << nextShotTick;
        ss << ", ";
        ss << "ammo: ";
        ss << "[ ";
        for (size_t ammoIndex = 0; ammoIndex < ammo.size(); ammoIndex++) {
            const int& ammoElement = ammo[ammoIndex];
            if (ammoIndex != 0) {
                ss << ", ";
            }
            ss << ammoElement;
        }
        ss << " ]";
        ss << ", ";
        ss << "shieldPotions: ";
        ss << shieldPotions;
        ss << " }";
        return ss.str();
    }

    void Unit::calcSpeedCircle(const model::Constants& constants) {
        unit_radius = constants.unitRadius;
        max_forward_speed = constants.maxUnitForwardSpeed;
        max_backward_speed = constants.maxUnitBackwardSpeed;
        if (weapon) {
            max_forward_speed *=  (1 - (1 - constants.weapons[*weapon].aimMovementSpeedModifier) * aim);
            max_backward_speed *=  (1 - (1 - constants.weapons[*weapon].aimMovementSpeedModifier) * aim);
        }

        speed_radius = (max_backward_speed + max_forward_speed) / 2;
        speed_center = position.clone() + direction.clone().mul((max_forward_speed - max_backward_speed) / 2);
    };

    void Unit::showSpeedCircle(DebugInterface *debugInterface) const {
        debugInterface->addRing(speed_center, speed_radius, 0.1, debugging::Color(0, 0, 0, 0.5));
        debugInterface->addPolyLine({position, position + velocity}, 0.1, debugging::Color(0, 1, 0, 1));
        debugInterface->addPolyLine({position, position + direction}, 0.1, debugging::Color(0, 1, 0, 1));

        std::stringstream ss;
        ss.precision(3);
        ss << aim;
        std::string text = ss.str() + " / " +  std::to_string(nextShotTick);
        debugInterface->addPlacedText(position, text, {0, -1}, 0.5, debugging::Color(0, 0, 0, 0.8));

    };

    model::Vec2 Unit::getVelocity(const model::Vec2& dir) const {
        double sin_a = std::clamp(direction.cross(dir), -1.0, 1.0);
        double d = (max_forward_speed - max_backward_speed) / 2;
        double sin_b = d * sin_a / speed_radius;
        double angle = M_PI - asin(sin_a) - asin(sin_b);

        double len = sqrt(d * d + speed_radius * speed_radius - 2 * d * speed_radius * cos(angle));
        return dir * len;
    }

    std::optional<double> Unit::hasHit(const model::Obstacle& obstacle) const {
        auto c0 = position - obstacle.position;
        double a = velocity.x * velocity.x + velocity.y * velocity.y;
        if (a < 1e-8) {
            return std::nullopt;
        }

        double b = 2 * c0.x * velocity.x + 2 * c0.y * velocity.y;
        double radius = obstacle.radius + MyStrategy::getConstants()->unitRadius;
        double c = c0.x * c0.x + c0.y * c0.y - radius * radius;
        double d = b * b - 4 * a * c;

        if (d < 0) {
            return std::nullopt;
        }

        double t = (-b - sqrt(d)) / 2.0 / a;

        if (t < 0 || t > 1.0 / MyStrategy::getConstants()->ticksPerSecond) {
            return std::nullopt;
        }

        return t;
    }

}