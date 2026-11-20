#pragma once

namespace nbody {

struct Vec3 {
    double x = 0;
    double y = 0;
    double z = 0;

    Vec3& operator+=(Vec3 other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
};

inline Vec3 operator-(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 operator*(Vec3 v, double scalar)
{
    return {v.x * scalar, v.y * scalar, v.z * scalar};
}

inline double dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

struct Particle {
    double mass;
    Vec3 position;
    Vec3 velocity;
    Vec3 acceleration{};
};

} // namespace nbody
