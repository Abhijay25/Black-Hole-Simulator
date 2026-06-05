#ifndef VEC3_H
#define VEC3_H

typedef struct {
    double x;
    double y;
    double z;
} Vec3;

Vec3 add(Vec3 vector1, Vec3 vector2);
Vec3 subtract(Vec3 vector1, Vec3 vector2);  
Vec3 scale(Vec3 vector, double scalar);
double dot(Vec3 vector1, Vec3 vector2);
double length(Vec3 vector);
Vec3 normalize(Vec3 vector);

#endif // VEC3_H