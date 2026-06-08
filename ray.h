#ifndef RAY_H
#define RAY_H
#define MAX_PATH 1000

#include "vec3.h"
#include "blackhole.h"

typedef struct {
    float x;
    float y;
    float z;

    float r;
    float phi;
    Vec3 direction;

    // Trail of the ray for visualization
    Vec3 path[MAX_PATH];
    int path_index;
} Photon;

Photon photon_init(Vec3 origin, Vec3 direction);
void photon_draw(Photon ray);
void step(Photon *ray, Blackhole *bh);

#endif // RAY_H