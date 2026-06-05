#include "raylib.h"


int main(void) {

  InitWindow(800, 600, "Blackhole Sim");
  int resolution  = 800 * 600;

  Color pixels[resolution];
  Texture2D texture;

  Image image = GenImageColor(800, 600, BLACK);
  texture = LoadTextureFromImage(image);

  UnloadImage(image);

  while (!WindowShouldClose()) {

    BeginDrawing();
      for (int i = 0; i < resolution; i++) {
      pixels[i] = (Color){255, 0, 255, 255};
      }
      UpdateTexture(texture, pixels);
      DrawTexturePro(texture,
                     (Rectangle) {0, 0, 800, 600},  //Source
                     (Rectangle) { 0, 0, GetScreenWidth(), GetScreenHeight()},  //Destionation
                     (Vector2){0, 0}, 0.0f, WHITE);  //Origin
    EndDrawing();
  }

  CloseWindow();

  return 0;
}
