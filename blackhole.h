#ifndef BLACKHOLE_H
#define BLACKHOLE_H
#include "vec3.h"

#define G 6.67430e-11 // Gravitational constant
#define C 299792458.0 // Speed of light in vacuum

typedef struct {
    Vec3 position;
    double mass;
    double EventHorizon;
} Blackhole;

Blackhole blackhole_init(Vec3 position, double mass);
void blackhole_draw(Blackhole bh);

#endif // BLACKHOLE_H