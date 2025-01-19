#include <ctype.h>
#include <emscripten.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>  // RB Added
#include <stdbool.h> // RB Added (debug/testing)
#include <stdlib.h>  // RB Added (debug/testing)
#include "doomgeneric.h"
#include "doomkeys.h"
#include "m_argv.h" // RB Added

static bool frame_initialized = false; // RB added (debug/testing)
uint32_t start_time;
int frame_count = 0;

uint32_t get_time()
{
  return EM_ASM_INT({
    return Date.now();
  });
}

// TODO: Uncomment timeout for sleep handling to limit DOM I think?
void DG_SleepMs(uint32_t ms)
{
  // FPS limiting for DOM efficiency
  // EM_ASM_({ return new Promise(resolve = > setTimeout(resolve, $0)); }, ms);
}

uint32_t DG_GetTicksMs()
{
  return get_time() - start_time;
}

// > > > RB Edits > > >
// Pre-allocated buffers to avoid per-frame allocations
static char ascii_rows[DOOMGENERIC_RESY][DOOMGENERIC_RESX + 1]; // +1 for null terminator
static const char ASCII_CHARS[] = "_::?//b#";
static const unsigned char THRESHOLDS[] = {200, 150, 100, 50, 25, 0};

// Lookup table for faster brightness calculations (pre-computed)
static unsigned char brightness_lookup[256]; // 256*3 for R+G+B combinations

// Initialize lookup tables
void DG_Init()
{
  // Initialize timing
  start_time = get_time();

  // First initialize the frame buffer in JavaScript
  // RB added (debug/testing)
  bool js_init = EM_ASM_INT({ return Module.initFrameBuffer($0, $1); }, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
  if (!js_init)
  {
    printf("Failed to initialize JavaScript frame buffer\n");
    return;
  }

  // Then cache the PDF fields
  // RB added (debug/testing)
  bool fields_cached = EM_ASM_INT({ return Module.cacheFields($0); }, DOOMGENERIC_RESY);

  if (!fields_cached)
  {
    printf("Failed to cache PDF fields\n");
    return;
  }

  // Initialize with final values after division
  for (int i = 0; i < 256; i++)
  {
    unsigned char bright = i >> 2; // Consistent with usage
    if (bright > 200)
      brightness_lookup[i] = 0; // "_"
    else if (bright > 150)
      brightness_lookup[i] = 1; // "::"
    else if (bright > 100)
      brightness_lookup[i] = 2; // "?"
    else if (bright > 50)
      brightness_lookup[i] = 3; // "//"
    else if (bright > 25)
      brightness_lookup[i] = 4; // "b"
    else
      brightness_lookup[i] = 5;
  }

  frame_initialized = true;
  printf("DoomGeneric initialized successfully\n");
}
// < < < RB Edits < < <

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
  int key_data = EM_ASM_INT({
    if (key_queue.length == = 0)
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

// > > > RB Edits > > >
void DG_DrawFrame()
{
  if (!frame_initialized)
  {
    printf("Cannot draw frame - not initialized\n");
    return;
  }

  // Handle key events first
  EM_ASM({
    for (let key of Object.keys(pressed_keys))
    {
      key_queue.push([ key, !!pressed_keys[key] ]);
      if (pressed_keys[key] == = 0)
        delete pressed_keys[key];
      if (pressed_keys[key] == = 2)
        pressed_keys[key] = 0;
    }
  });

  // Convert RGB to ASCII
  for (int y = 0; y < DOOMGENERIC_RESY; y++)
  {
    uint32_t *row_start = &DG_ScreenBuffer[y * DOOMGENERIC_RESX];
    char *ascii_row = ascii_rows[y];

    // Latest optimized ASCII conversion
    /*for (int x = 0; x < DOOMGENERIC_RESX; x++)
    {
      uint32_t pixel = row_start[x];
      // Add RGB and lookup directly - no second division needed
      unsigned char bright = ((pixel >> 16) & 0xFF) + // col shifted to R
                             ((pixel >> 8) & 0xFF) +  // col shifted to G
                             (pixel & 0xFF);          // col shifted to B
      // Div by 3 to get average and scale to 0-255 range(from upwards of 750)
      bright /= 3;
      ascii_row[x] = ASCII_CHARS[brightness_lookup[bright]];
    }*/
    // Simplified ASCII conversion (for debug/testing)
    for (int y = 0; y < DOOMGENERIC_RESY; y++)
    {
      uint32_t *row_start = &DG_ScreenBuffer[y * DOOMGENERIC_RESX];
      char *ascii_row = ascii_rows[y];

      for (int x = 0; x < DOOMGENERIC_RESX; x++)
      {
        uint32_t pixel = row_start[x];
        unsigned char r = (pixel >> 16) & 0xFF;
        unsigned char g = (pixel >> 8) & 0xFF;
        unsigned char b = pixel & 0xFF;
        unsigned char bright = (r + g + b) / 3;
        ascii_row[x] = ASCII_CHARS[brightness_lookup[bright]];
      }
      ascii_row[DOOMGENERIC_RESX] = '\0'; // Null terminate
    }

    // Direct update with length instead of null terminator
    // Optimized version (maybe this itself not optimal, but was part of the optim code avobe)
    /*EM_ASM({
            // Field access with error checking
            const fieldName = "field_" + ($3 - $2 - 1);
            const field = globalThis.getField(fieldName);
            if (!field)
            {
              console.error("Field not found:", fieldName);
              return;
            }

            // Only update if the row has changed
            const rowStr = UTF8ToString($0, $1);
            if (field.value !== rowStr) {
                field.value = rowStr;
            } }, ascii_row, DOOMGENERIC_RESX, y, DOOMGENERIC_RESY);
  }*/
    // Update the frame in JavaScript
    bool update_success = EM_ASM_INT({ return Module.updateFrame($0, $1, $2); }, ascii_rows, DOOMGENERIC_RESX, DOOMGENERIC_RESY);

    if (!update_success)
    {
      printf("Frame update failed\n");
    }
  }
  // < < < RB Edits < < <

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
    EM_ASM({ create_framebuffer($0, $1); }, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
    printf("Loading resources...\n");

    EM_ASM({
      write_file(file_name, file_data);
      if (file2_data)
      {
        write_file(file2_name, file2_data);
      }
    });
    printf("Resources loaded\n");

    doomgeneric_Create(argc, argv);
    printf("Starting main loop\n");

    EM_ASM({
      app.setInterval("_doomjs_tick()", 0);
    });
    printf("Main loop started\n");
    return 0;
  }