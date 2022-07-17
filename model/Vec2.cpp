#include "Vec2.hpp"

namespace model {
    Vec2::Vec2() : x(0), y(0) { }
    Vec2::Vec2(double x, double y) : x(x), y(y) { }

    // Read Vec2 from input stream
    Vec2 Vec2::readFrom(InputStream& stream) {
        double x = stream.readDouble();
        double y = stream.readDouble();
        return Vec2(x, y);
    }

    // Write Vec2 to output stream
    void Vec2::writeTo(OutputStream& stream) const {
        stream.write(x);
        stream.write(y);
    }

    // Get string representation of Vec2
    std::string Vec2::toString() const {
        std::stringstream ss;
        ss << "Vec2 { ";
        ss << "x: ";
        ss << x;
        ss << ", ";
        ss << "y: ";
        ss << y;
        ss << " }";
        return ss.str();
    }

    double Vec2::distTo(const Vec2& vec) const {
        return sqrt((x - vec.x) * (x - vec.x) + (y - vec.y) * (y - vec.y));
    }

    double Vec2::distToSquared(const Vec2& vec) const {
        return (x - vec.x) * (x - vec.x) + (y - vec.y) * (y - vec.y);
    }

    double Vec2::len() const {
        return sqrt(x * x + y * y);
    }

    bool Vec2::isEmpty() {
        return x * x + y * y < 1e-9;
    }

    Vec2& Vec2::norm() {
        double l = len();

        if (l < 1e-8) {
            return *this;
        }

        x /= l;
        y /= l;

        return *this;
    }

    Vec2& Vec2::rotate(const double angle) {
        double new_x = x * cos(angle) - y * sin(angle);
        double new_y = x * sin(angle) + y * cos(angle);
        x = new_x;
        y = new_y;

        return *this;
    }

    Vec2& Vec2::mul(const double& scalar) {
        x *= scalar;
        y *= scalar;

        return *this;
    }

    Vec2& Vec2::div(const double& scalar) {
        x /= scalar;
        y /= scalar;

        return *this;
    }

    Vec2& Vec2::add(const Vec2& vec) {
        x += vec.x;
        y += vec.y;

        return *this;
    }

    Vec2 Vec2::clone() const {
        return {x, y};
    }
}