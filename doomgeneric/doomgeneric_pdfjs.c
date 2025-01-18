#include <ctype.h>
#include <emscripten.h>
#include <stdio.h>
#include <unistd.h>
#include "doomgeneric.h"
#include "doomkeys.h"
#include "m_argv.h" // RB Added
#include <string.h> // RB Added

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

// > > > RB Edits > > >
// Pre-allocated buffers to avoid per-frame allocations
static char ascii_rows[DOOMGENERIC_RESY][DOOMGENERIC_RESX + 1]; // +1 for null terminator
static const char ASCII_CHARS[] = "_::?//b#";
static const unsigned char THRESHOLDS[] = {200, 150, 100, 50, 25, 0};

// Lookup table for faster brightness calculations (pre-computed)
static unsigned char brightness_lookup[768]; // 256*3 for R+G+B combinations

// Initialize lookup tables
void DG_Init()
{
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
  // Tell JS how many rows we have
  EM_ASM({
    // window is SDL_Window for Emscripten
    window.totalRows = $0;
    window.rowCache = new Array($0).fill(""); // Double quotes here
  },
         DOOMGENERIC_RESY);
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

// Move to a separate debug function
void DG_DebugInfo()
{
  static int last_frame_time = 0;
  int current_time = get_time();
  if (frame_count % 60 == 0)
  {
    int frame_time = current_time - last_frame_time;
    EM_ASM({ console.log(`Frame ${$0} : avg time ${$1} ms`); }, frame_count, frame_time / 60);
  }
  last_frame_time = current_time;
}

// > > > RB Edits > > >
void DG_DrawFrame()
{
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

  for (int y = 0; y < DOOMGENERIC_RESY; y++)
  {
    uint32_t *row_start = &DG_ScreenBuffer[y * DOOMGENERIC_RESX];
    char *ascii_row = ascii_rows[y];

    for (int x = 0; x < DOOMGENERIC_RESX; x++)
    {
      uint32_t pixel = row_start[x];
      // Add RGB and lookup directly - no second division needed
      unsigned char bright = ((pixel >> 16) & 0xFF) +
                             ((pixel >> 8) & 0xFF) +
                             (pixel & 0xFF);
      ascii_row[x] = ASCII_CHARS[brightness_lookup[bright]];
    }

    // Direct update with length instead of null terminator
    EM_ASM({
            const rowStr = UTF8ToString($0, $1);
            const field = globalThis.getField("field_" + ($3-$2-1));
            if (field.value !== rowStr) {
                field.value = rowStr;
            } }, ascii_row, DOOMGENERIC_RESX, y, DOOMGENERIC_RESY);
  }

  // Tick frame timer and print debug info
  DG_DebugInfo();
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