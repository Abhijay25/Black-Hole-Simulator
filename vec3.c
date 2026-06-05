#include "vec3.h"
#include "math.h"

Vec3 add(Vec3 vector1, Vec3 vector2) {
    Vec3 result;
    result.x = vector1.x + vector2.x;
    result.y = vector1.y + vector2.y;
    result.z = vector1.z + vector2.z;
    return result;
}

Vec3 subtract(Vec3 vector1, Vec3 vector2) {
    Vec3 result;
    result.x = vector1.x - vector2.x;
    result.y = vector1.y - vector2.y;
    result.z = vector1.z - vector2.z;
    return result;
}

Vec3 scale(Vec3 vector, double scalar) {
    Vec3 result;
    result.x = vector.x * scalar;
    result.y = vector.y * scalar;
    result.z = vector.z * scalar;
    return result;
}

double dot(Vec3 vector1, Vec3 vector2) {
    return vector1.x * vector2.x + vector1.y * vector2.y + vector1.z * vector2.z;
}

double length(Vec3 vector) {
    return sqrt(dot(vector, vector));
}

Vec3 normalize(Vec3 vector) {
    double len = length(vector);
    if (len < 1e-10) { // Avoid division by zero
        return (Vec3){0, 0, 0};
    }
    return scale(vector, 1.0 / len);
}
