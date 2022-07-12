#ifndef __MODEL_RAY_HPP__
#define __MODEL_RAY_HPP__

#include "model/Vec2.hpp"
#include <cmath>

namespace model {

class Ray {
public:
    model::Vec2 dir;
    model::Vec2 origin;

    Ray(const model::Vec2& _origin, const model::Vec2& _dir);

    double distanceSqToPoint(const Vec2& point) const;
    double distanceToPoint(const Vec2& point) const;
    double intersectsCircle(const model::Vec2& center, double radius) const;

};

}

#endif