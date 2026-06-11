# Black-Hole-Simulator
Inspired by https://youtu.be/8-B6ryuBkCM?si=Mh6mT4aQZD1P4biH 

Made this start learning C while also making something fun.

## Tech Stack

- **C** — core simulation and rendering glue
- **raylib 5.5** — windowing, input, 3D rasterization (spacetime mesh), texture compositing
- **OpenGL 4.3 compute shader** (GLSL) — GPU geodesic ray tracing for the black hole shadow, gravitationally lensed accretion disk, and lensed background objects (`geodesic.comp`)
- **Nix flake** — reproducible dev environment (`flake.nix`)
- **Make** — build (`Makefile`)

Physics: full Schwarzschild geodesics integrated per-photon with RK4. Each ray is solved in its own orbital plane (2D equatorial reduction) to avoid the polar coordinate singularity.

## Build & Run

```sh
nix develop      # enter the dev shell (provides gcc, raylib, libGL)
make
./blackhole      # move the mouse to orbit; Esc to quit
```

## Configuration

All tunable knobs live in `main.c` unless noted. Values in the compute path are expressed in multiples of `rs_c` (the on-screen Schwarzschild radius).

| What to change | Where | How |
|---|---|---|
| **Number of objects spawned** | `main.c`, `spawn_objects(&uboData, 5, rs_c);` | Change the `5` to any count up to `MAX_OBJECTS` (16). Each is placed at a random direction, 6–12·rs from the BH, with random size and colour. |
| **Accretion ring thickness** | `main.c`, `.diskThick = 0.065f * rs_c` | Raise for fatter, more pronounced lensed rings; lower for a razor-thin disk. |
| **Disk inner/outer radius** | `main.c`, `.diskR1 = 1.5f * rs_c`, `.diskR2 = 4.0f * rs_c` | Inner edge / outer edge of the disk, in units of rs. |
| **Grid size** (mesh extent) | `main.c`, `#define GRID_SIZE 200.0f` | Half-width of the spacetime mesh in world units. |
| **Grid step size** (mesh density) | `main.c`, `float step = 7.5f;` (in the mesh loop) | Smaller = denser/finer grid lines (more draw calls); larger = coarser. |
| **Swap M87 ↔ Sag A\*** | `main.c`, the `RENDER_SCALE` / `BH_MASS` `#define` block near the top | M87 is active; comment it out and uncomment the Sag A\* pair (it ships with a matching `RENDER_SCALE` so the smaller BH stays on-screen). |
| **Camera orbit distance** | `main.c`, `float orbitRadius = 350.0f;` | Distance of the camera from the black hole; lower to zoom in, raise to pull back. |

### Other knobs worth knowing
- **Compute resolution** — `#define COMPUTE_W/COMPUTE_H` in `main.c`. Higher = sharper lensing but more GPU cost (the ray tracer runs one ray per compute texel).
- **Fixed vs. random object layout** — `srand((unsigned)time(NULL));` in `main.c`. Comment it out for the same layout every run.
- **Integration accuracy/reach** — `STEP_FACTOR`, `MAX_STEPS`, `STEP_MIN/MAX` in `geodesic.comp`.
