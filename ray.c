#include "ray.h"
#include "raylib.h"
#include "math.h"
#include "string.h"

Photon photon_init(Vec3 position, Vec3 direction, double EventHorizon) {
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

    double f = 1 - EventHorizon / ray.r; //Schwarzschild factor
    double dt_dlambda = sqrt(ray.dr * ray.dr / f + 
        ray.r * ray.r * ray.dtheta * ray.dtheta + 
        ray.r * ray.r * sin(ray.theta) * sin(ray.theta) * ray.dphi * ray.dphi); // Rate of Change of Coordinate Time with respect to Affine Parameter
    ray.E = f * dt_dlambda; // Conserved Energy for Photon

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
    
    // Update the photon's position and velocity using RK4 integration
    rk4step(ray, lambda, bh);
   
    // Conversion of Polar to Cartesian Coordinates for Drawing 
    ray->x = cos(ray->phi) * sin(ray->theta) *ray->r;
    ray->y = sin(ray->phi) * sin(ray->theta) * ray->r;
    ray->z = cos(ray->theta) * ray->r;

    // Add current position to path for trail visualization
    ray->path[ray->path_index] = (Vec3){ray->x, ray->y, ray->z};
    ray->path_index = (ray->path_index + 1) % MAX_PATH;
    if (ray->trail_count < MAX_PATH) ray->trail_count++;
}

void geodesic(Photon *ray, double rhs[6], Blackhole *bh) {
    //Create copies of values to avoid modifying the original ray during calculations
    double r = ray->r;
    double theta = ray->theta;

    double dr = ray->dr;
    double dphi = ray->dphi;
    double dtheta = ray->dtheta;

    rhs[0] = dr;
    rhs[1] = dphi;
    rhs[2] = dtheta;

    double rs = bh->EventHorizon;

    double f;
    if (r > rs) {
            f = 1.0 - rs / r;          // Schwarzschild factor: approaches 0 at event horizon, causing extreme curvature
    } else {
        memset(rhs, 0, 6 * sizeof(double)); // If inside event horizon, no acceleration (Zero out all Values)
        return;
    }
    double dt_dlambda = ray->E / f;   // Recover coordinate time rate from conserved energy

    // Full Schwarzschild null geodesic accelerations
    rhs[3] = -(rs / (2.0 * r * r)) * f * dt_dlambda * dt_dlambda + (rs / (2.0 * r * r * f)) * dr * dr + r * (dtheta * dtheta + sin(theta) * sin(theta) * dphi * dphi); // Radial acceleration
    rhs[4] = (-2.0 / r) * dr * dphi - 2.0 * (cos(theta) / sin(theta)) * dtheta * dphi; // Phi acceleration
    rhs[5] = (-2.0 / r) * dr * dtheta + sin(theta) * cos(theta) * dphi * dphi; // Theta acceleration
}

// Runge-Kutta 4th order method for updating the photon's position and velocity
void rk4step(Photon *ray, double lambda, Blackhole *bh) {
    double rayVals[6] = { ray->r, ray->phi, ray->theta, ray->dr, ray->dphi, ray->dtheta };
    double k1[6], k2[6], k3[6], k4[6], temp[6];

    // Find the Estimates of the 4 curves, then Average out to get Final Curve for Photon's Next Step
    // First Curve Estiamtion
    geodesic(ray, k1, bh);
    addState(rayVals, k1, lambda * 0.5, temp);
    Photon ray2 = *ray; ray2.r = temp[0]; ray2.phi = temp[1]; ray2.theta = temp[2]; ray2.dr = temp[3]; ray2.dphi = temp[4]; ray2.dtheta = temp[5];
    
    // Second Curve Estimation
    geodesic(&ray2, k2, bh);

    addState(rayVals, k2, lambda * 0.5, temp);
    Photon ray3 = *ray; ray3.r = temp[0]; ray3.phi = temp[1]; ray3.theta = temp[2]; ray3.dr = temp[3]; ray3.dphi = temp[4]; ray3.dtheta = temp[5];
    
    // Third Curve Estimation
    geodesic(&ray3, k3, bh);

    addState(rayVals, k3, lambda, temp);
    Photon ray4 = *ray; ray4.r = temp[0]; ray4.phi = temp[1]; ray4.theta = temp[2]; ray4.dr = temp[3]; ray4.dphi = temp[4]; ray4.dtheta = temp[5];
    
    // Fourth Curve Estimation
    geodesic(&ray4, k4, bh);

    //Average out to Find the Final Curve for the Photon's Next Step
    ray->r      += (lambda / 6.0) * (k1[0] + 2*k2[0] + 2*k3[0] + k4[0]);
    ray->phi    += (lambda / 6.0) * (k1[1] + 2*k2[1] + 2*k3[1] + k4[1]);
    ray->theta  += (lambda / 6.0) * (k1[2] + 2*k2[2] + 2*k3[2] + k4[2]);
    ray->dr     += (lambda / 6.0) * (k1[3] + 2*k2[3] + 2*k3[3] + k4[3]);
    ray->dphi   += (lambda / 6.0) * (k1[4] + 2*k2[4] + 2*k3[4] + k4[4]);
    ray->dtheta += (lambda / 6.0) * (k1[5] + 2*k2[5] + 2*k3[5] + k4[5]);
}

void addState(const double a[6], const double b[6], double factor, double out[6]) {
      for (int i = 0; i < 6; i++)
          out[i] = a[i] + b[i] * factor;
  }