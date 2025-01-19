#include <ctype.h>
#include <emscripten.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h> // **Added to declare memset and memcpy**
#include "doomgeneric.h"
#include "doomkeys.h"

uint32_t start_time;
int frame_count = 0;

uint32_t get_time()
{
  return EM_ASM_INT({
    return Date.now();
  });
}

void DG_SleepMs(uint32_t ms) {}

uint32_t DG_GetTicksMs()
{
  return get_time() - start_time;
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
  int key_data = EM_ASM_INT({
    if (key_queue.length === 0)
      return 0;
    let key_data = key_queue.shift();
    let key = key_data[0];
    let pressed = key_data[1];
    return (pressed << 8) | key;
  });

  if (key_data == 0)
    return 0;

  *pressed = key_data >> 8;
  *doomKey = key_data & 0xFF;
  return 1;
}

void DG_SetWindowTitle(const char *title) {}

int key_to_doomkey(int key)
{
  if (key == 97) // a
    return KEY_LEFTARROW;
  if (key == 100) // d
    return KEY_RIGHTARROW;
  if (key == 119) // w
    return KEY_UPARROW;
  if (key == 115) // s
    return KEY_DOWNARROW;
  if (key == 113) // q
    return KEY_ESCAPE;
  if (key == 122) // z
    return KEY_ENTER;
  if (key == 101) // e
    return KEY_USE;
  if (key == 32) //<space>
    return KEY_FIRE;
  if (key == 109) //,
    return KEY_TAB;
  if (key == 95) //_
    return KEY_RSHIFT;
  return tolower(key);
}

// ==================== SHADING ====================
// =================================================
// Pre-computed ASCII shading LUT
// Single-byte ASCII shading characters from lightest to darkest
static const char BLOCK_CHARS[4] = {' ', '.', ':', '#'}; // Single-byte ASCII chars

static uint8_t *ascii_buffer = NULL;
static size_t ascii_buffer_size = 0;
static uint32_t last_frame_time = 0;
static const uint32_t TARGET_FRAME_TIME = 33; // ~30 FPS target

// Fast RGB to brightness conversion using bit shifts
#define RGB_TO_BRIGHTNESS(r, g, b) (((r) + (g) + (b)) >> 2)

void DG_Init()
{
  start_time = get_time();

  // Adjust buffer size to accommodate line breaks if implemented in C
  ascii_buffer_size = (DOOMGENERIC_RESX + 1) * DOOMGENERIC_RESY; // Only 1 byte per char + newlines
  ascii_buffer = (uint8_t *)malloc(ascii_buffer_size);

  if (!ascii_buffer)
  {
    printf("Failed to allocate ASCII buffer.\n");
    exit(1);
  }

  // Pre-warm the buffer with spaces and newlines
  for (int y = 0; y < DOOMGENERIC_RESY; y++)
  {
    memset(&ascii_buffer[y * (DOOMGENERIC_RESX + 1)], 0, DOOMGENERIC_RESX);
    ascii_buffer[y * (DOOMGENERIC_RESX + 1) + DOOMGENERIC_RESX] = '\n';
  }
}

void DG_DrawFrame()
{
  uint32_t current_time = get_time();
  uint32_t frame_delta = current_time - last_frame_time;

  // 1. Process input queue
  EM_ASM({
    for (let key of Object.keys(pressed_keys))
    {
      key_queue.push([ key, !!pressed_keys[key] ]);
      if (pressed_keys[key] === 0)
        delete pressed_keys[key];
      if (pressed_keys[key] === 2)
        pressed_keys[key] = 0;
    }
  });

  // Render Frame-rate Limiting
  if (frame_delta < TARGET_FRAME_TIME)
  {
    return;
  }
  last_frame_time = current_time;

  // 2. Convert framebuffer to ASCII art on the C side
  const uint32_t *src = (uint32_t *)DG_ScreenBuffer;
  uint8_t *dst = ascii_buffer;

  // No OpenMP pragmas for simplicity
  for (int y = 0; y < DOOMGENERIC_RESY; y++)
  {
    for (int x = 0; x < DOOMGENERIC_RESX; x++)
    {
      uint32_t pixel = src[y * DOOMGENERIC_RESX + x];
      uint8_t r = (pixel >> 16) & 0xFF;
      uint8_t g = (pixel >> 8) & 0xFF;
      uint8_t b = pixel & 0xFF;

      // Fast brightness calculation
      uint8_t brightness = RGB_TO_BRIGHTNESS(r, g, b);
      uint8_t shade_index = brightness >> 6;             // Results in 0-3
      shade_index = (shade_index > 3) ? 3 : shade_index; // Add safety bound

      // Map brightness to ASCII character
      char shade_char = BLOCK_CHARS[shade_index];
      dst[y * (DOOMGENERIC_RESX + 1) + x] = shade_char;
    }

    // Add newline character at the end of each row
    dst[y * (DOOMGENERIC_RESX + 1) + DOOMGENERIC_RESX] = '\n';
  }

  // 3. Send pre-computed ASCII buffer to JavaScript
  EM_ASM({ update_ascii_frame($0, $1, $2, $3); }, ascii_buffer, ascii_buffer_size, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
}

// ==================== MAIN ====================
// ==============================================
// Timer tick and main functions

void doomjs_tick()
{
  int start = get_time();
  doomgeneric_Tick();
  int end = get_time();
  frame_count++;

  if (frame_count % 30 == 0)
  {
    int fps = 1000 / (end - start);
    printf("frame time: %i ms (%i fps)\n", end - start, fps);
  }
}

int main(int argc, char **argv)
{
	// N/A (implemented in c, here)
  // EM_ASM({ create_framebuffer($0, $1); }, DOOMGENERIC_RESX, DOOMGENERIC_RESY);

  EM_ASM({
    write_file(file_name, file_data);
    if (file2_data)
    {
      write_file(file2_name, file2_data);
    }
  });

  doomgeneric_Create(argc, argv);

  EM_ASM({
    app.setInterval("_doomjs_tick()", 0);
  });
  return 0;
}