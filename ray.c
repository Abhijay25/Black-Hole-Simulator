#include "ray.h"
#include "raylib.h"

Ray ray_init(Vec3 origin, Vec3 direction) {
    Ray ray;
    ray.origin = origin;
    ray.direction = normalize(direction);
    ray.path_index = 0;
    return ray;
}

void draw(Ray ray) {
    float opacity = 255;
    float decrement = 255.0f / MAX_PATH;
    int idx = (ray.path_index - 1 + MAX_PATH) % MAX_PATH; // -1 to reset to last used

    for (int i = 0; i < MAX_PATH; i++) {
        Color c = {(unsigned char)opacity, (unsigned char)opacity, (unsigned char)opacity, 255};
        DrawPixel((int)ray.path[idx].x, (int)ray.path[idx].y, c);
        opacity -= decrement;
        idx = (idx - 1 + MAX_PATH) % MAX_PATH; // Decrement to last recently used position
    }
    DrawCircle((int)ray.origin.x, (int)ray.origin.y, 2.0f, WHITE);
}

void step(Ray *ray) {
    ray->path[ray->path_index] = ray->origin;
    ray->path_index = (ray->path_index + 1) % MAX_PATH;
    
    ray->origin.x += ray->direction.x * C;
    ray->origin.y += ray->direction.y * C;
    ray->origin.z += ray->direction.z * C;
}