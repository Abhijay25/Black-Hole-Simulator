#include "ray.h"
#include "raylib.h"
#include "math.h"

Photon photon_init(Vec3 position, Vec3 direction) {
    Photon ray;
    ray.x = position.x;
    ray.y = position.y;
    ray.z = position.z;

    ray.r = sqrtf(ray.x * ray.x + ray.y * ray.y + ray.z * ray.z);
    ray.phi = atan2f(ray.y, ray.x);

    ray.direction = normalize(direction);
    ray.path_index = 0;
    return ray;
}

void photon_draw(Photon ray) {
    float opacity = 255;
    float decrement = 255.0f / MAX_PATH;
    int idx = (ray.path_index - 1 + MAX_PATH) % MAX_PATH; // -1 to reset to last used

    for (int i = 0; i < MAX_PATH; i++) {
        Color c = {(unsigned char)opacity, (unsigned char)opacity, (unsigned char)opacity, 255};
        DrawPixel((int)ray.path[idx].x, (int)ray.path[idx].y, c);
        opacity -= decrement;
        idx = (idx - 1 + MAX_PATH) % MAX_PATH; // Decrement to last recently used position
    }
    DrawCircle((int)ray.x, (int)ray.y, 2.0f, WHITE);
}

void step(Photon *ray, Blackhole *bh) {
    // If Photon crosses into Blackhole, disappear
    ray->r = sqrtf(ray->x * ray->x + ray->y * ray->y + ray->z * ray->z);
    if (ray->r < bh->EventHorizon) return;

    ray->path[ray->path_index] = (Vec3){ray->x, ray->y, ray->z};
    ray->path_index = (ray->path_index + 1) % MAX_PATH;
    
    ray->x += ray->direction.x * C;
    ray->y += ray->direction.y * C;
    ray->z += ray->direction.z * C;
}