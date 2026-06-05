#include "blackhole.h"
#include "raylib.h"

Blackhole blackhole_init(Vec3 position, double mass) {
    Blackhole bh;
    bh.position = position;
    bh.mass = mass;

    bh.EventHorizon = 2.0 * G * mass / (C * C);
    return bh;
}

void draw(Blackhole bh) {
    DrawCircle((int)bh.position.x, (int)bh.position.y, (float)bh.EventHorizon, BLACK);
}