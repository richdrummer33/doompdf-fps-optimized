#ifndef DOOM_GENERIC
#define DOOM_GENERIC

#include <stdlib.h>
#include <stdint.h>

#define HALF_SCALE// RB For Ascii or PDF

// ######## RB HALVED VALUES ######## 
#ifndef DOOMGENERIC_RESX
#ifdef HALF_SCALE
#define DOOMGENERIC_RESX 160
#else
#define DOOMGENERIC_RESX 160 
#endif // HALF_SCALE
#endif // DOOMGENERIC_RESX

#ifndef DOOMGENERIC_RESY
#ifdef HALF_SCALE
#define DOOMGENERIC_RESY 100
#else
#define DOOMGENERIC_RESY 200
#endif
#endif

// #define DOUBLE_SCALE

#ifdef CMAP256

typedef uint8_t pixel_t;

#else // CMAP256

typedef uint32_t pixel_t;

#endif // CMAP256

extern pixel_t *DG_ScreenBuffer;

void doomgeneric_Create(int argc, char **argv);
void doomgeneric_Tick();

// Implement below functions for your platform
void DG_Init();
void DG_DrawFrame();
void DG_SleepMs(uint32_t ms);
uint32_t DG_GetTicksMs();
int DG_GetKey(int *pressed, unsigned char *key);
void DG_SetWindowTitle(const char *title);

#endif // DOOM_GENERIC
