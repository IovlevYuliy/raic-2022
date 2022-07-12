#ifndef __MODEL_VEC2_HPP__
#define __MODEL_VEC2_HPP__

#include "stream/Stream.hpp"
#include <sstream>
#include <string>
#include <cmath>

namespace model {

// 2 dimensional vector.
class Vec2 {
public:
    // `x` coordinate of the vector
    double x;
    // `y` coordinate of the vector
    double y;

    Vec2();

    Vec2(double x, double y);

    // Read Vec2 from input stream
    static Vec2 readFrom(InputStream& stream);

    // Write Vec2 to output stream
    void writeTo(OutputStream& stream) const;

    // Get string representation of Vec2
    std::string toString() const;

    double distTo(const Vec2& vec) const;

    double distToSquared(const Vec2& vec) const;

    double len() const;

    Vec2& norm();
    Vec2& mul(const double& scalar);
    Vec2& div(const double& scalar);
    Vec2 clone() const;

    double dot(const Vec2& vec) const {
        return x * vec.x + y * vec.y;
    }

    double cross(const Vec2& vec) const {
        return x * vec.y - y * vec.x;
    }

    void rotate(const double angle);

    Vec2& operator +=(const Vec2& vec) {
        x += vec.x;
        y += vec.y;

        return *this;
    }

    Vec2 operator +(const Vec2& vec) const {
        return Vec2(x + vec.x, y + vec.y);
    }

    Vec2 operator -(const Vec2& vec) const {
        return Vec2(x - vec.x, y - vec.y);
    }

    Vec2 operator *(const double& scalar) const {
        return Vec2(scalar * x, scalar * y);
    }
};

}

#endif