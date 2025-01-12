#include <emscripten.h>
#include "doomgeneric.h"

uint32_t start_time;

uint32_t get_time() {
  return EM_ASM_INT({
    return Date.now();
  });
}

void DG_SleepMs(uint32_t ms) {}


uint32_t DG_GetTicksMs() {
  return get_time() - start_time;
}

void DG_Init() {
  start_time = get_time();
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
  return 0;
}

void DG_SetWindowTitle(const char * title) {}

void DG_DrawFrame() {
  int framebuffer_len = DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4;
  EM_ASM({
    var framebuffer_ptr = $0;
    var framebuffer_len = $1;
    var width = $2;
    var height = $3;
    var framebuffer = Module.HEAPU8.subarray(framebuffer_ptr, framebuffer_ptr + framebuffer_len);
    for (var y=0; y < height; y++) {
      var row = Array(width);
      for (var x=0; x < width; x++) {
        var index = (y * width + x) * 4;
        var r = framebuffer[index];
        var g = framebuffer[index+1];
        var b = framebuffer[index+2];
        var avg = (r + g + b) / 3;
        if (avg > 80) 
          row[x] = "_";
        else
          row[x] = "#";
      }
      globalThis.getField("field_"+(height-y-1)).value = row.join("");
    }
  }, DG_ScreenBuffer, framebuffer_len, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
}

void doomjs_tick() {
  doomgeneric_Tick();
}

int main(int argc, char **argv) {
  doomgeneric_Create(argc, argv);

  EM_ASM({
    app.setInterval("_doomjs_tick()", 0);
  });
  return 0;
}