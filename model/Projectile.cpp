#include "Projectile.hpp"
#include "MyStrategy.hpp"

namespace model {

Projectile::Projectile(int id, int weaponTypeIndex, int shooterId, int shooterPlayerId, model::Vec2 position, model::Vec2 velocity, double lifeTime) : id(id), weaponTypeIndex(weaponTypeIndex), shooterId(shooterId), shooterPlayerId(shooterPlayerId), position(position), velocity(velocity), lifeTime(lifeTime) { }
    // Read Projectile from input stream
    Projectile Projectile::readFrom(InputStream& stream) {
        int id = stream.readInt();
        int weaponTypeIndex = stream.readInt();
        int shooterId = stream.readInt();
        int shooterPlayerId = stream.readInt();
        model::Vec2 position = model::Vec2::readFrom(stream);
        model::Vec2 velocity = model::Vec2::readFrom(stream);
        double lifeTime = stream.readDouble();
        return Projectile(id, weaponTypeIndex, shooterId, shooterPlayerId, position, velocity, lifeTime);
    }

    // Write Projectile to output stream
    void Projectile::writeTo(OutputStream& stream) const {
        stream.write(id);
        stream.write(weaponTypeIndex);
        stream.write(shooterId);
        stream.write(shooterPlayerId);
        position.writeTo(stream);
        velocity.writeTo(stream);
        stream.write(lifeTime);
    }

    // Get string representation of Projectile
    std::string Projectile::toString() const {
        std::stringstream ss;
        ss << "Projectile { ";
        ss << "id: ";
        ss << id;
        ss << ", ";
        ss << "weaponTypeIndex: ";
        ss << weaponTypeIndex;
        ss << ", ";
        ss << "shooterId: ";
        ss << shooterId;
        ss << ", ";
        ss << "shooterPlayerId: ";
        ss << shooterPlayerId;
        ss << ", ";
        ss << "position: ";
        ss << position.toString();
        ss << ", ";
        ss << "velocity: ";
        ss << velocity.toString();
        ss << ", ";
        ss << "lifeTime: ";
        ss << lifeTime;
        ss << " }";
        return ss.str();
    }

    bool Projectile::intersectUnit(const Unit& unit, const Constants& constants) const {
        model::Ray ray(position, velocity);

        if (!ray.intersectsCircle(unit.position, constants.unitRadius)) {
            return false;
        }

        double dist_to_unit = position.distTo(unit.position) - constants.unitRadius;
        if (dist_to_unit / constants.weapons[weaponTypeIndex].projectileSpeed > lifeTime) {
            return false;
        }

        return true;
    }

    bool Projectile::intersectCircle(const Vec2& center, const double radius) const {
        model::Ray ray(position, velocity);
        return ray.intersectsCircle(center, radius);
    }

    bool Projectile::hasHit(const model::Unit& unit) const {
        auto c0 = position - unit.position;
        auto v = velocity - unit.velocity;
        double a = v.x * v.x + v.y * v.y;
        double b = 2 * c0.x * v.x + 2 * c0.y * v.y;
        double c = c0.x * c0.x + c0.y * c0.y - unit.unit_radius * unit.unit_radius;
        double d = b * b - 4 * a * c;

        if (d < 0) {
            return false;
        }

        double t = (-b + sqrt(d)) / 2.0 / a;

        if (t < 0 || t > 1.0 / MyStrategy::getConstants()->ticksPerSecond || t > lifeTime) {
            return false;
        }

        return true;
    }

}