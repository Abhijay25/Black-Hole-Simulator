#ifndef RAY_H
#define RAY_H
#define MAX_PATH 100

#include "vec3.h"
#include "blackhole.h"

typedef struct {
    double meters_per_pixel;
    double origin_x;  // screen position of world (0,0)
    double origin_y;
} Viewport;

typedef struct {
    float x; float y; float z;
    float r; float phi; float theta;      // Polar Coordinates
    float dr; float dphi; float dtheta;   // Polar Velocities
    float d2r; float d2phi; float d2theta;// Polar Accelerations

    Vec3 direction;
    Vec3 path[MAX_PATH];
    int path_index;
    int trail_count;
} Photon;

Photon photon_init(Vec3 origin, Vec3 direction);
void photon_draw(Photon ray, Viewport vp);
void step(Photon *ray, Blackhole *bh, double lambda);
void geodesic(Photon *ray, Blackhole *bh);

#endif // RAY_H