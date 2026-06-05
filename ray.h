#ifndef RAY_H
#define RAY_H
#define MAX_PATH 1000

#include "vec3.h"
#include "blackhole.h"

typedef struct {
    Vec3 origin;
    Vec3 direction;
    Vec3 path[MAX_PATH];
    int path_index;
} Ray;

Ray ray_init(Vec3 origin, Vec3 direction);
void draw(Ray ray);
void step(Ray *ray);

#endif // RAY_H