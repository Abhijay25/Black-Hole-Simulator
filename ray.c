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
    ray.theta = acos(ray.z / ray.r);

    ray.dr = direction.x * cos(ray.phi) + direction.y * sin(ray.phi);
    ray.dphi = (direction.y * cos(ray.phi) - direction.x * sin(ray.phi)) / ray.r;
    ray.dtheta = (cos(ray.theta)*cos(ray.phi)*direction.x + cos(ray.theta)*sin(ray.phi)*direction.y - sin(ray.theta)*direction.z) / ray.r;

    ray.d2r = 0.0f; ray.d2phi = 0.0f; ray.d2theta = 0.0f;

    ray.direction = direction;
    ray.path_index = 0;
    ray.trail_count = 0;
    return ray;
}

static inline Vector2 world_to_screen(double wx, double wy, Viewport vp) {
    return (Vector2){
        (float)(vp.origin_x + wx / vp.meters_per_pixel),
        (float)(vp.origin_y - wy / vp.meters_per_pixel)
    };
}

void photon_draw(Photon ray, Viewport vp) {
    int count = ray.trail_count < MAX_PATH ? ray.trail_count : MAX_PATH;
    if (count < 2) return;

    float opacity = 255.0f;
    float decrement = 255.0f / (float)(count - 1);

    int idx_a = (ray.path_index - 1 + MAX_PATH) % MAX_PATH;
    int idx_b = (ray.path_index - 2 + MAX_PATH) % MAX_PATH;

    for (int i = 0; i < count - 1; i++) {
        Vector2 a = world_to_screen(ray.path[idx_a].x, ray.path[idx_a].y, vp);
        Vector2 b = world_to_screen(ray.path[idx_b].x, ray.path[idx_b].y, vp);
        DrawLineEx(a, b, 2.0f, (Color){255, 255, 255, (unsigned char)opacity});
        opacity -= decrement;
        idx_a = idx_b;
        idx_b = (idx_b - 1 + MAX_PATH) % MAX_PATH;
    }
}

void step(Photon *ray, Blackhole *bh, double lambda) {
    // If Photon crosses into Blackhole, disappear
    ray->r = sqrtf(ray->x * ray->x + ray->y * ray->y + ray->z * ray->z);
    if (ray->r < bh->EventHorizon) return;
    
    ray->dr += ray->d2r * lambda;
    ray->dphi += ray->d2phi * lambda;
    ray->dtheta += ray->d2theta * lambda;

    ray->r += ray->dr * lambda;
    ray->phi += ray->dphi * lambda;
    ray->theta += ray->dtheta * lambda;
   
    ray->x = cos(ray->phi) * sin(ray->theta) *ray->r;
    ray->y = sin(ray->phi) * sin(ray->theta) * ray->r;
    ray->z = cos(ray->theta) * ray->r;

    ray->path[ray->path_index] = (Vec3){ray->x, ray->y, ray->z};
    ray->path_index = (ray->path_index + 1) % MAX_PATH;
    if (ray->trail_count < MAX_PATH) ray->trail_count++;
}

void geodesic(Photon *ray, Blackhole *bh) {
    //Create copies of values to avoid modifying the original ray during calculations
    double r = ray->r;
    double theta = ray->theta;

    double dr = ray->dr;
    double dphi = ray->dphi;
    double dtheta = ray->dtheta;

    ray->d2r = r * dphi * dphi - (C*C * bh->EventHorizon) / (2.0 * r * r); // How fast we approach the black hole
    ray->d2phi = -2.0 * dr * dphi / r; // How fast the angle changes
    ray->d2theta = (-2.0 * dr * dtheta) / r + sin(theta) * cos(theta) * dphi * dphi;
}