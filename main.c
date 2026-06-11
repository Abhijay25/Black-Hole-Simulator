#include "raylib.h"
#include "blackhole.h"
#include "ray.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#define SCREEN_W   800
#define SCREEN_H   600
#define GRID_SIZE  200.0f
#define COMPUTE_W  480
#define COMPUTE_H  384
#define RENDER_SCALE 1e12f  // 1 world unit = 1 trillion metres M87
#define BH_MASS 1.29e40 // M87

// #define RENDER_SCALE 1e9f   // 1 world unit = 1 billion metres Sag A*
// #define BH_MASS 4.15e36 // Sag A*

typedef struct {
    float camPos[4];
    float camRight[4];
    float camUp[4];
    float camFwd[4];
    float objPosRadius[3][4];  // per object: xyz = centre, w = radius
    float objColor[3][4];      // per object: rgb = colour
    float tanHalfFov;
    float aspect;
    float rs;
    float diskR1;
    float diskR2;
    float diskThick;
    float _pad[2];
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
        float innerR = 1.5f * rs;
        float outerR = 4.0f * rs;
        float slices = 64.0f;
        float sliceStep = 2 * M_PI / slices;

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

    // Compute shader: GPU geodesic ray tracing for BH shadow + lensed disk
    float rs_c    = (float)(black_hole.EventHorizon / RENDER_SCALE);
    float diskR1  = 1.5f * rs_c;
    float diskR2  = 4.0f * rs_c;
    float diskThk = 0.05f * rs_c;

    // Background spheres (lensed by the BH). Each row: x, y, z, radius.
    // Tweak positions/sizes/colours here.
    Vector3 fwd   = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, camera.up));
    Vector3 up    = Vector3Normalize(Vector3CrossProduct(right, fwd));

    CameraUBOData uboData = {
      .camPos       = { camera.position.x, camera.position.y, camera.position.z, 0 },
      .camRight     = { right.x, right.y, right.z, 0 },
      .camUp        = { up.x, up.y, up.z, 0 },
      .camFwd       = { fwd.x, fwd.y, fwd.z, 0 },
      .objPosRadius = {
        {  8.0f*rs_c,  2.0f*rs_c, -3.0f*rs_c, 1.6f*rs_c },  // big
        { -6.0f*rs_c,  4.0f*rs_c,  5.0f*rs_c, 0.9f*rs_c },  // medium
        {  2.0f*rs_c, -3.0f*rs_c,  9.0f*rs_c, 0.5f*rs_c },  // small
      },
      .objColor = {
        { 0.90f, 0.12f, 0.08f, 1.0f },  // red
        { 0.30f, 0.55f, 1.00f, 1.0f },  // blue
        { 0.55f, 0.95f, 0.35f, 1.0f },  // green
      },
      .tanHalfFov   = tanf(camera.fovy * 0.5f * DEG2RAD),
      .aspect       = (float)COMPUTE_W / COMPUTE_H,
      .rs           = rs_c,
      .diskR1       = diskR1,
      .diskR2       = diskR2,
      .diskThick    = diskThk,
    };
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
