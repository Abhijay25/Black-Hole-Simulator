#include "raylib.h"
#include "blackhole.h"
#include "ray.h"

#define SCREEN_W         1280
#define SCREEN_H         720
#define METERS_PER_PIXEL 1.5e8
#define BH_SCREEN_X      (SCREEN_W * 0.75)
#define BH_SCREEN_Y      (SCREEN_H * 0.5)

int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "Blackhole Sim");
    SetTargetFPS(60);

    Viewport vp = { METERS_PER_PIXEL, BH_SCREEN_X, BH_SCREEN_Y };

#define NUM_PHOTONS 20

    Blackhole bh = blackhole_init((Vec3){0, 0, 0}, 8.54e36);

    Photon photons[NUM_PHOTONS];
    for (int i = 0; i < NUM_PHOTONS; i++) {
        double screen_y = (double)SCREEN_H * i / (NUM_PHOTONS - 1);
        double world_y  = (BH_SCREEN_Y - screen_y) * METERS_PER_PIXEL;
        photons[i] = photon_init(
            (Vec3){-BH_SCREEN_X * METERS_PER_PIXEL, world_y, 0},
            (Vec3){C, 0, 0}
        );
    }

    int   bh_px   = (int)BH_SCREEN_X;
    int   bh_py   = (int)BH_SCREEN_Y;
    float bh_r_px = (float)(bh.EventHorizon / METERS_PER_PIXEL);

    while (!WindowShouldClose()) {
        for (int i = 0; i < NUM_PHOTONS; i++) {
            geodesic(&photons[i], &bh);
            step(&photons[i], &bh, 1.0);
        }

        BeginDrawing();
            ClearBackground(BLACK);
            DrawCircle(bh_px, bh_py, bh_r_px, RED);
            for (int i = 0; i < NUM_PHOTONS; i++)
                photon_draw(photons[i], vp);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
