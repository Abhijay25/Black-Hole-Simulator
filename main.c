#include "raylib.h"
#include "blackhole.h"
#include "ray.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#define SCREEN_W   800
#define SCREEN_H   600
#define GRID_SIZE  200.0f
#define COMPUTE_W  480
#define COMPUTE_H  384
#define MAX_OBJECTS 16
#define RENDER_SCALE 1e12f  // 1 world unit = 1 trillion metres M87
#define BH_MASS 1.29e40 // M87

// #define RENDER_SCALE 1e9f   // 1 world unit = 1 billion metres Sag A*
// #define BH_MASS 4.15e36 // Sag A*

typedef struct {
    float camPos[4];
    float camRight[4];
    float camUp[4];
    float camFwd[4];
    float objPosRadius[16][4];  // per object: xyz = centre, w = radius
    float objColor[16][4];      // per object: rgb = colour
    float tanHalfFov;
    float aspect;
    float rs;
    float diskR1;
    float diskR2;
    float diskThick;
    int   numObjects;
    float _pad;
} CameraUBOData;

static GLuint loadComputeShader(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { printf("Cannot open %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *src = (char *)malloc(len + 1);
    fread(src, 1, len, f);
    src[len] = '\0';
    fclose(f);

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    const GLchar *s = src;
    glShaderSource(shader, 1, &s, NULL);
    glCompileShader(shader);
    free(src);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, 2048, NULL, log);
        printf("Compute shader compile error:\n%s\n", log);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, shader);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, 2048, NULL, log);
        printf("Compute program link error:\n%s\n", log);
        return 0;
    }
    glDeleteShader(shader);
    return prog;
}

// Fill the first `n` object slots with constrained-random spheres: a safe
// distance from the BH (so none start inside the lensing/horizon region),
// a sane size range, and a random bright colour. `rs` is the render-scale
// Schwarzschild radius; all distances are expressed in multiples of it.
static float frand(void) { return (float)rand() / (float)RAND_MAX; }

static void spawn_objects(CameraUBOData *d, int n, float rs) {
    if (n > MAX_OBJECTS) n = MAX_OBJECTS;
    if (n < 0) n = 0;
    for (int i = 0; i < n; i++) {
        // Random direction on the unit sphere (uniform)
        float u   = frand() * 2.0f - 1.0f;     // cos(polar)
        float phi = frand() * 2.0f * (float)M_PI;
        float s   = sqrtf(1.0f - u*u);
        Vector3 dir = { s * cosf(phi), u, s * sinf(phi) };

        float dist   = (6.0f + frand() * 6.0f) * rs;   // 6..12 rs from BH
        float radius = (0.4f + frand() * 1.2f) * rs;   // 0.4..1.6 rs

        d->objPosRadius[i][0] = dir.x * dist;
        d->objPosRadius[i][1] = dir.y * dist;
        d->objPosRadius[i][2] = dir.z * dist;
        d->objPosRadius[i][3] = radius;

        // Random bright colour (each channel 0.35..1.0)
        d->objColor[i][0] = 0.35f + frand() * 0.65f;
        d->objColor[i][1] = 0.35f + frand() * 0.65f;
        d->objColor[i][2] = 0.35f + frand() * 0.65f;
        d->objColor[i][3] = 1.0f;
    }
    d->numObjects = n;
}

int main(void) {
  // Camera Setup
  Camera3D camera;
  camera.position = (Vector3){10, 10, 10};
  camera.target = (Vector3){0, 0, 0};
  camera.fovy = 45.0f;
  camera.up = (Vector3){0, 1, 0};
  camera.projection = CAMERA_PERSPECTIVE;

  // Spherical orbit coordinates (radians)
  float orbitYaw   = 0.785f;   // azimuth (around Y axis)
  float orbitPitch = 0.2f;     // elevation
  float orbitRadius = 350.0f;  // distance from origin

  // Window Initialization
  SetConfigFlags(FLAG_WINDOW_HIGHDPI);  // use the real framebuffer on scaled displays
  InitWindow(SCREEN_W, SCREEN_H, "Black Hole Simulation");
  DisableCursor();
  SetTargetFPS(30);

  // True framebuffer size in physical pixels (differs from SCREEN_W/H under
  // fractional display scaling). Draw the final composite at this size so it
  // fills the whole window instead of a top-left corner.
  int fbW = GetRenderWidth();
  int fbH = GetRenderHeight();
  printf("logical=%dx%d  framebuffer=%dx%d  scale=%.2f,%.2f\n",
         GetScreenWidth(), GetScreenHeight(), fbW, fbH,
         GetWindowScaleDPI().x, GetWindowScaleDPI().y);

  // Render target for the spacetime mesh scene (sized to real framebuffer)
  RenderTexture2D renderTarget = LoadRenderTexture(fbW, fbH);

  // Compute shader + output texture
  GLuint computeProg = loadComputeShader("geodesic.comp");

  GLuint computeTex;
  glGenTextures(1, &computeTex);
  glBindTexture(GL_TEXTURE_2D, computeTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, COMPUTE_W, COMPUTE_H,
               0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);

  GLuint ubo;
  glGenBuffers(1, &ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBOData), NULL, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  // Wrap compute output for raylib drawing
  Texture2D computeDisplayTex = {
    .id      = computeTex,
    .width   = COMPUTE_W,
    .height  = COMPUTE_H,
    .mipmaps = 1,
    .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
  };

  // Initialize the Black Hole
  Blackhole black_hole = blackhole_init((Vec3){0, 0, 0}, BH_MASS);

  // Static scene data for the compute shader (BH + disk + objects). Filled
  // once here; only the camera fields are updated each frame.
  float rs_c = (float)(black_hole.EventHorizon / RENDER_SCALE);
  CameraUBOData uboData = {
    .tanHalfFov = tanf(camera.fovy * 0.5f * DEG2RAD),
    .aspect     = (float)COMPUTE_W / COMPUTE_H,
    .rs         = rs_c,
    .diskR1     = 1.5f * rs_c,
    .diskR2     = 4.0f * rs_c,
    .diskThick  = 0.065f * rs_c,
  };
  srand((unsigned)time(NULL));   // remove for a fixed layout each run
  spawn_objects(&uboData, 5, rs_c);   // change 5 to spawn more/fewer

  int firstFrame = 1;  // ignore the startup mouse spike

  while(!WindowShouldClose()) {
    // Orbit camera with raw mouse movement (no click required)
    Vector2 mouseDelta = GetMouseDelta();
    if (firstFrame) { mouseDelta = (Vector2){0, 0}; firstFrame = 0; }
    orbitYaw   += mouseDelta.x * 0.004f;
    orbitPitch -= mouseDelta.y * 0.004f;
    if (orbitPitch >  1.4f) orbitPitch =  1.4f;  // prevent flipping over poles
    if (orbitPitch < -1.4f) orbitPitch = -1.4f;

    camera.position = (Vector3){
      orbitRadius * cosf(orbitPitch) * sinf(orbitYaw),
      orbitRadius * sinf(orbitPitch),
      orbitRadius * cosf(orbitPitch) * cosf(orbitYaw)
    };

    // Render Scene to Texture
    BeginTextureMode(renderTarget);
      ClearBackground(BLACK);
      BeginMode3D(camera);

        // Space-Time Mesh
        float step = 7.5f;
        float rs = (float)(black_hole.EventHorizon / RENDER_SCALE); // Schwarzschild radius
        float k = 2.0f * sqrtf(rs);  // Distortion strength

        for (float x = -GRID_SIZE; x <= GRID_SIZE; x += step) {
          for (float z = -GRID_SIZE; z <= GRID_SIZE; z += step) {
            float r = sqrtf(x*x + z*z);
            if (r <= rs) continue; // inside event horizon, skip
            float distortion = -k * sqrtf(rs / (r - rs)); // vertical distortion based on distance to event horizon

            // Horizontal line (along X)
            float r2 = sqrtf((x+step)*(x+step) + z*z);
            if (r2 > rs) {
              float y2 = -k * sqrtf(rs / (r2 - rs));
              DrawLine3D((Vector3){x, distortion, z}, (Vector3){x+step, y2, z}, GRAY);
            }

            // Vertical line (along Z)
            float r3 = sqrtf(x*x + (z+step)*(z+step));
            if (r3 > rs) {
              float y3 = -k * sqrtf(rs / (r3 - rs));
              DrawLine3D((Vector3){x, distortion, z}, (Vector3){x, y3, z+step}, GRAY);
            }
          }
        }

        // Initialize the Black Hole
        DrawSphere((Vector3){0, 0, 0}, rs, BLACK);

        // Accretion Disc
        // float innerR = 1.5f * rs;
        // float outerR = 4.0f * rs;
        // float slices = 64.0f;
        // float sliceStep = 2 * M_PI / slices;

        // Split the Disc into Trapezoids
        // Render Trapezoid as 2 Triangles
        // for (float angle = 0; angle <= 2* M_PI; angle += sliceStep) {
        //   Vector3 innerLeftCorner = (Vector3){innerR * cosf(angle), 0, innerR * sinf(angle)};
        //   Vector3 innerRightCorner = (Vector3){innerR * cosf(angle + sliceStep), 0, innerR * sinf(angle + sliceStep)};
        //   Vector3 outerLeftCorner = (Vector3){outerR * cosf(angle), 0, outerR * sinf(angle)};
        //   Vector3 outerRightCorner = (Vector3){outerR * cosf(angle + sliceStep), 0, outerR * sinf(angle + sliceStep)};

        //   // Draw the 2 Triangles (bottom face)
        //   DrawTriangle3D(innerLeftCorner, outerLeftCorner, innerRightCorner, ORANGE);
        //   DrawTriangle3D(outerLeftCorner, outerRightCorner, innerRightCorner, ORANGE);
        //   // Draw the 2 Triangles (top face, reversed winding)
        //   DrawTriangle3D(innerRightCorner, outerLeftCorner, innerLeftCorner, ORANGE);
        //   DrawTriangle3D(innerRightCorner, outerRightCorner, outerLeftCorner, ORANGE);
        // }

      EndMode3D();
    EndTextureMode();

    // Compute shader: update only the camera fields each frame (scene data
    // and objects were filled once before the loop).
    Vector3 fwd   = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, camera.up));
    Vector3 up    = Vector3Normalize(Vector3CrossProduct(right, fwd));

    uboData.camPos[0]   = camera.position.x;
    uboData.camPos[1]   = camera.position.y;
    uboData.camPos[2]   = camera.position.z;
    uboData.camRight[0] = right.x; uboData.camRight[1] = right.y; uboData.camRight[2] = right.z;
    uboData.camUp[0]    = up.x;    uboData.camUp[1]    = up.y;    uboData.camUp[2]    = up.z;
    uboData.camFwd[0]   = fwd.x;   uboData.camFwd[1]   = fwd.y;   uboData.camFwd[2]   = fwd.z;

    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBOData), &uboData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo);

    glUseProgram(computeProg);
    glBindImageTexture(0, computeTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glDispatchCompute((COMPUTE_W + 15) / 16, (COMPUTE_H + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    glUseProgram(0);

    // Composite: mesh background + ray-traced overlay
    BeginDrawing();
      DrawTextureRec(renderTarget.texture,
                     (Rectangle){0, 0, fbW, -fbH},
                     (Vector2){0, 0}, WHITE);
      BeginBlendMode(BLEND_ALPHA);
        DrawTexturePro(computeDisplayTex,
                       (Rectangle){0, 0, COMPUTE_W, -COMPUTE_H},
                       (Rectangle){0, 0, fbW, fbH},
                       (Vector2){0, 0}, 0.0f, WHITE);
      EndBlendMode();
    EndDrawing();
  }

  UnloadRenderTexture(renderTarget);
  glDeleteTextures(1, &computeTex);
  glDeleteBuffers(1, &ubo);
  glDeleteProgram(computeProg);
  CloseWindow();
  return 0;
}
