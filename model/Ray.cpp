#include "Ray.hpp"

namespace model {

Ray::Ray(const model::Vec2& _origin, const model::Vec2& _dir): origin(_origin), dir(_dir) {
    dir.norm();
}

double Ray::distanceSqToPoint(const model::Vec2& point) const {
    auto dirDist = (point - origin).dot(dir);
    if (dirDist < 0) {
        return origin.distToSquared(point);
    }

	auto vec = dir * dirDist + origin;

    return vec.distToSquared(point);
};

double Ray::distanceToPoint(const model::Vec2& point) const {
    return sqrt(distanceSqToPoint(point));
};

double Ray::intersectsCircle(const model::Vec2& center, double radius) const {
    return distanceSqToPoint(center) <= (radius * radius);
};

}