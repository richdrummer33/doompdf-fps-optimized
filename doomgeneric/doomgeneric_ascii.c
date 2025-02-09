//
// Copyright(C) 2022-2024 Wojciech Graj
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     terminal-specific code
//


// Richard Beare 2025
/* 
 _______________________________
	 ___NOTEPAD++ SETTINGS___
 ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 * Language ➡️ XML
 * Settings ➡️ Style Configurator:
 *		Theme: Deep Black
 *		Language: Global Styles
 *		Style: Default Style
 *		Colour Style:
 *		  - Foreground: White (#FFFFFF)
 *		  - Background: Black (#000000)
 *		Font Style:
 *		  - Font Name: MingLiU-ExtB
 *		  - Bold: No
 *		  - Italic: No
 *		  - Underline: No
 *		  - Font Size: 9
 * 
 * ---------------------------------------
 * Screeenshot: https://imgur.com/a/2Qpy8S5
 */


#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_system.h"
#include "i_video.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32

#if defined(_WIN32) || defined(WIN32)
#define OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
// #define USE_COLOR
// Display modes
#define DISP_CLIPBOARD
// #define DISP_NOTEPAD_PP // notepad++ mode
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

#ifdef DISP_NOTEPAD_PP
#define SCI_SETTEXT 2181
#define SCI_GOTOLINE 2024

// Optional: More useful Scintilla messages you might want
#define SCI_GETTEXTLENGTH 2182
#define SCI_SETREADONLY 2171
#define SCI_GETREADONLY 2140
#define SCI_CLEARALL 2004
#define SCI_GETLENGTH 2006
#define SCI_SELECTALL 2013
#define SCI_DOCUMENTEND 2318
#define SCI_DOCUMENTSTART 2316
#endif // DISP_NOTEPAD_PP

#ifdef  DISP_CLIPBOARD
// Define virtual key codes for F13-F24
#define VK_F13 0x7C
#define VK_F14 0x7D
#define VK_F15 0x7E
#define VK_F16 0x7F
#define VK_F17 0x80
#define VK_F18 0x81
#define VK_F19 0x82
#define VK_F20 0x83
#define VK_F21 0x84
#define VK_F22 0x85
#define VK_F23 0x86
#define VK_F24 0x87
// Scintilla
#define SCI_SELECTALL 2013
#define SCI_CLEARALL 2004
#endif //  DISP_CLIPBOARD


#ifdef OS_WINDOWS
#include <consoleapi2.h> // rb for console/cmd cursor pos top-left

#define CLK 0

#define WINDOWS_CALL(cond, format)        \
	do {                              \
		if (UNLIKELY(cond))       \
			winError(format); \
	} while (0)

static void winError(char* const format)
{
	LPVOID lpMsgBuf;
	const DWORD dw = GetLastError();
	errno = dw;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);
	I_Error(format, lpMsgBuf);
}

/* Modified from https://stackoverflow.com/a/31335254 */
struct timespec {
	long tv_sec;
	long tv_nsec;
};
int clock_gettime(const int p, struct timespec* const spec)
{
	(void)p;
	__int64 wintime;
	GetSystemTimeAsFileTime((FILETIME*)&wintime);
	wintime -= 116444736000000000ll;
	spec->tv_sec = wintime / 10000000ll;
	spec->tv_nsec = wintime % 10000000ll * 100;
	return 0;
}

#else
#define CLK CLOCK_REALTIME
#endif

#ifdef __GNUC__
#define UNLIKELY(x) __builtin_expect((x), 0)
#else
#define UNLIKELY(x) (x)
#endif

#define CALL(stmt, format)                      \
	do {                                    \
		if (UNLIKELY(stmt))             \
			I_Error(format, errno); \
	} while (0)
#define CALL_STDOUT(stmt, format) CALL((stmt) == EOF, format)

#define BYTE_TO_TEXT(buf, byte)                      \
	do {                                         \
		*(buf)++ = '0' + (byte) / 100u;      \
		*(buf)++ = '0' + (byte) / 10u % 10u; \
		*(buf)++ = '0' + (byte) % 10u;       \
	} while (0)

// #define GRAD_LEN 70u // RB replaced with static field
#define INPUT_BUFFER_LEN 16u
#define EVENT_BUFFER_LEN ((INPUT_BUFFER_LEN)*2u - 1u)

// Equal-width gradient art but it sucks
//static const char grad[] = "\u200B\u200B__~~<<~~TTFFHH####"; //░░▒▒▓▓████████"; 
static const char grad[] = "  __--<<\\/\\/~~##░░▒▒▓▓████████";
// Some bullshit
// static const char grad[] =  " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

static size_t grad_len;  // Excludes the null terminator, set in init
int frame_count = 0;

#ifdef DISP_CLIPBOARD
// String for foreground window
wchar_t gameWindowStr[30] = { 0 };	// Allocate
wchar_t currWindowStr[30] = { 1 };  // Allocate
// Key to start/stop game ticks
char key_doUpdate = ']';
char key_dontUpdate = '[';
// Flag to start rendering
BOOL doUpdate= FALSE;
BOOL wasPaused = FALSE;
// Flag to indicate if the window that we pressed 'key_doUpdate' on (started the game in) is 
BOOL isFocused = TRUE;
#endif

struct color_t {
	uint32_t b : 8;
	uint32_t g : 8;
	uint32_t r : 8;
	uint32_t a : 8;
};

static char* output_buffer;
static size_t output_buffer_size;
static struct timespec ts_init;

static uint32_t sum_frame_time = 0;
static uint32_t last_time = 0;
const int fps_log_interval = 60; // frames
const int TARGET_FRAME_TIME = 60; // FPS=1/FRAMETIME

static unsigned char input_buffer[INPUT_BUFFER_LEN];
static uint16_t event_buffer[EVENT_BUFFER_LEN];
static uint16_t* event_buf_loc;

DWORD pressedKey; // RB for global KB hooks

// Global renderer instance for DOOM
#ifdef  DISP_NOTEPAD_PP
typedef struct {
	HWND notepad;
	HWND scintilla;
	char* buffer;
	size_t buffer_size;
} AsciiRenderer;

static AsciiRenderer* g_renderer = NULL;
#endif s


void DG_AtExit(void)
{
#ifdef OS_WINDOWS
	DWORD mode;
	const HANDLE hInputHandle = GetStdHandle(STD_INPUT_HANDLE);
	if (UNLIKELY(hInputHandle == INVALID_HANDLE_VALUE))
		return;
	if (UNLIKELY(!GetConsoleMode(hInputHandle, &mode)))
		return;
	mode |= ENABLE_ECHO_INPUT;
	SetConsoleMode(hInputHandle, mode);
#else
	struct termios t;
	if (UNLIKELY(tcgetattr(STDIN_FILENO, &t)))
		return;
	t.c_lflag |= ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &t);
#endif
}

void DG_Init()
{
	grad_len = strlen(grad); // RB for pixel-ascii list sizing

#ifdef OS_WINDOWS
	const HANDLE hOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	WINDOWS_CALL(hOutputHandle == INVALID_HANDLE_VALUE, "DG_Init: %s");
	DWORD mode;
	WINDOWS_CALL(!GetConsoleMode(hOutputHandle, &mode), "DG_Init: %s");
	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	WINDOWS_CALL(!SetConsoleMode(hOutputHandle, mode), "DG_Init: %s");

	const HANDLE hInputHandle = GetStdHandle(STD_INPUT_HANDLE);
	WINDOWS_CALL(hInputHandle == INVALID_HANDLE_VALUE, "DG_Init: %s");
	WINDOWS_CALL(!GetConsoleMode(hInputHandle, &mode), "DG_Init: %s");
	mode &= ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_QUICK_EDIT_MODE | ENABLE_ECHO_INPUT);
	WINDOWS_CALL(!SetConsoleMode(hInputHandle, mode), "DG_Init: %s");
#else
	struct termios t;
	CALL(tcgetattr(STDIN_FILENO, &t), "DG_Init: tcgetattr error %d");
	t.c_lflag &= ~(ECHO);
	CALL(tcsetattr(STDIN_FILENO, TCSANOW, &t), "DG_Init: tcsetattr error %d");
#endif
	CALL(atexit(&DG_AtExit), "DG_Init: atexit error %d");

	/* Longest SGR code: \033[38;2;RRR;GGG;BBBm (length 19)
	 * Maximum 21 bytes per pixel: SGR + 2 x char
	 * 1 Newline character per line
	 * SGR clear code: \033[0m (length 4)
	 */
#ifdef USE_COLOR 
	 // 21u represents the maximum number of bytes needed per pixel for the ASCII color escape sequence
	 // +4u is for the start/end color-escape sequences + newline 
	output_buffer_size = 21u * DOOMGENERIC_RESX * DOOMGENERIC_RESY + DOOMGENERIC_RESY + 4u;
#else
	 // RB: orig is 4 bytes per "pixel" for regular no-color ascii
	output_buffer_size = 4u * DOOMGENERIC_RESX * DOOMGENERIC_RESY + DOOMGENERIC_RESY + 1;  // only need +1 for \n termination, no color terms
#endif
#ifdef  DISP_CLIPBOARD
	// For spacing between subsequent pasted frames that stack with intermittent clears (autoscroll in noteopad)
	output_buffer_size += 4u * DOOMGENERIC_RESY; // picking RESY frame-height for # of \ns to print at start of each frame
#endif //  DISP_CLIPBOARD

	output_buffer = malloc(output_buffer_size);
	clock_gettime(CLK, &ts_init);
	memset(input_buffer, '\0', INPUT_BUFFER_LEN);
}

HWND GetWin()
{
	HWND hwnd = GetForegroundWindow();
	if (!hwnd) hwnd = GetFocus();
	return hwnd;
}

void simulate_key_combo(WORD key1, WORD key2) {
	// Press key1 (e.g., VK_CONTROL for Ctrl)
	INPUT inputs[4] = { 0 };
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = key1;

	// Press key2 (e.g., 'V' for Ctrl+V)
	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = key2;

	// Release key2
	inputs[2].type = INPUT_KEYBOARD;
	inputs[2].ki.wVk = key2;
	inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

	// Release key1
	inputs[3].type = INPUT_KEYBOARD;
	inputs[3].ki.wVk = key1;
	inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

	// Send the inputs
	SendInput(4, inputs, sizeof(INPUT));
}

// SIMULATE F-13 for: 1. Win Select-ALL, 2. DELETE
BOOL init_clear_hotkeys() {
	// Register F13, F14, F15 for different operations
	BOOL success = TRUE;
	success &= RegisterHotKey(NULL, 1, 0, VK_F13);          // Clear text
	success &= RegisterHotKey(NULL, 2, MOD_SHIFT, VK_F13);  // Alternative clear method
	return success;
}

void cleanup_clear_hotkeys() {
	UnregisterHotKey(NULL, 1);
	UnregisterHotKey(NULL, 2);
}

void process_hotkey(int id) {
	printf("process_hotkey: %d key\n", id);

	HWND hwnd = GetForegroundWindow();
	if (!hwnd) return;

	HWND focused = GetFocus();
	if (!focused)
	{
		focused = hwnd;
		hwnd = focused;
	}

	switch (id) {
	case 1: // F13 - Select All + Delete 
	{
		INPUT inputs[4];
		ZeroMemory(inputs, sizeof(inputs));

		// Ctrl Down
		inputs[0].type = INPUT_KEYBOARD;
		inputs[0].ki.wVk = VK_CONTROL;

		// A Down
		inputs[1].type = INPUT_KEYBOARD;
		inputs[1].ki.wVk = 'A';

		// A Up
		inputs[2].type = INPUT_KEYBOARD;
		inputs[2].ki.wVk = 'A';
		inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

		// Ctrl Up
		inputs[3].type = INPUT_KEYBOARD;
		inputs[3].ki.wVk = VK_CONTROL;
		inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

		SendInput(4, inputs, sizeof(INPUT));
		Sleep(50);

		// Delete key
		INPUT del;
		ZeroMemory(&del, sizeof(del));
		del.type = INPUT_KEYBOARD;
		del.ki.wVk = VK_DELETE;
		SendInput(1, &del, sizeof(INPUT));

		del.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &del, sizeof(INPUT));
	}
	break;
	}
}

#ifdef DISP_CLIPBOARD

const int copy_maxAttempts = 5;

// Function to copy text to the clipboard
void copy_to_clipboard(const char* text)
{
	// TRY open clipboard
	if (!OpenClipboard(NULL)) 
	{
		// fprintf(stderr, "Failed to open clipboard.\n");
		int attempts = 0;
		while (!OpenClipboard(NULL))
		{
			Sleep(TARGET_FRAME_TIME / copy_maxAttempts *2);
			if (attempts > 5)
				return;
		}
		// printf("OpenClipboard recovered!");
	}

	// Empty the clipboard
	if (!EmptyClipboard()) {
		fprintf(stderr, "Failed to empty clipboard.\n");
		CloseClipboard();
		return;
	}

	// Get the length of the text
	size_t len = strlen(text) + 1;

	// Allocate global memory for the text
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
	if (!hMem) {
		fprintf(stderr, "Failed to allocate global memory.\n");
		CloseClipboard();
		return;
	}

	// Lock the global memory and copy the text
	void* ptr = GlobalLock(hMem);
	if (ptr) {
		memcpy(ptr, text, len);
		GlobalUnlock(hMem);

		// Set the clipboard data
		if (!SetClipboardData(CF_TEXT, hMem)) {
			fprintf(stderr, "Failed to set clipboard data.\n");
		}
	}
	else {
		fprintf(stderr, "Failed to lock global memory.\n");
		GlobalFree(hMem);
	}

	// Close the clipboard
	CloseClipboard();
}

void select_all_simple() {
	INPUT inputs[4] = { 0 };
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = VK_HOME;

	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = VK_SHIFT;

	inputs[2].type = INPUT_KEYBOARD;
	inputs[2].ki.wVk = VK_END;

	SendInput(3, inputs, sizeof(INPUT));
	Sleep(10);

	// Release shift
	inputs[3].type = INPUT_KEYBOARD;
	inputs[3].ki.wVk = VK_SHIFT;
	inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &inputs[3], sizeof(INPUT));
}

void paste_to_active_window() {
	// ============
	// CONSIDER FRAME INTERPOLATION FOR FIRING WEAPON https://claude.ai/chat/f7f809ec-df09-4ae2-8e46-ec5ad3ea1da9 
	// ============

	HWND hwnd = GetForegroundWindow();
	if (!hwnd) return;

	HWND focused = GetFocus();
	if (!focused)
	{
		focused = hwnd;
		hwnd = focused;
	}

	// Try multiple paste methods
	if (SendMessage(focused, WM_PASTE, 0, 0) > 0) {
		// printf("SM1 Paste success!\n");
		return;
	}
	if (SendMessage(focused, WM_APPCOMMAND, 0, MAKELPARAM(0, APPCOMMAND_PASTE)) > 0) {
		// printf("SM2 Paste success!\n");
		return;
	}

	// Sleep(10);
	// Try Ctrl+V simulation
	INPUT inputs[4] = { 0 };
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = VK_CONTROL;
	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = 'V';
	inputs[2].type = INPUT_KEYBOARD;
	inputs[2].ki.wVk = 'V';
	inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
	inputs[3].type = INPUT_KEYBOARD;
	inputs[3].ki.wVk = VK_CONTROL;
	inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(4, inputs, sizeof(INPUT));
	// printf("SM ctrl+v sim\n");
}

#else 
#if DISP_NOTEPAD_PP
HWND find_notepad_plus_plus() {
	HWND hwnd = FindWindow(L"Notepad++", NULL);
	if (hwnd == NULL) {
		printf("Could not find Notepad++ window\n");
		return NULL;
	}
	return hwnd;
}

// Find the Scintilla edit component within Notepad++
HWND find_scintilla(HWND notepad_hwnd) {
	// Notepad++ uses Scintilla as its edit component
	HWND scintilla = FindWindowEx(notepad_hwnd, NULL, L"Scintilla", NULL);
	if (scintilla == NULL) {
		printf("Could not find Scintilla component\n");
		return NULL;
	}
	return scintilla;
}


int DG_InitNotepadRenderer() {
	printf("Initializing Notepad++ renderer...\n");

	size_t width = DOOMGENERIC_RESX * 2;
	size_t height = DOOMGENERIC_RESY;
	size_t buffer_size = (width + 1) * height;
#ifdef USE_COLOR
	buffer_size += (width * height * 20);
#endif
	buffer_size += 100;

	printf("Allocating buffer of size: %zu bytes\n", buffer_size);

	g_renderer = (AsciiRenderer*)malloc(sizeof(AsciiRenderer));
	if (!g_renderer) {
		printf("Failed to allocate renderer structure\n");
		return 0;
	}

	g_renderer->notepad = find_notepad_plus_plus();
	if (!g_renderer->notepad) {
		printf("Could not find Notepad++ window\n");
		free(g_renderer);
		return 0;
	}

	g_renderer->scintilla = find_scintilla(g_renderer->notepad);
	if (!g_renderer->scintilla) {
		printf("Could not find Scintilla component\n");
		free(g_renderer);
		return 0;
	}

	g_renderer->buffer = (char*)malloc(buffer_size);
	if (!g_renderer->buffer) {
		printf("Failed to allocate render buffer\n");
		free(g_renderer);
		return 0;
	}

	g_renderer->buffer_size = buffer_size;
	memset(g_renderer->buffer, 0, buffer_size);

	// Initialize render timing
	last_time = GetTickCount();

	printf("Notepad++ renderer initialized successfully\n");
	return 1;
}

// Cleanup function - call this when shutting down DOOM
void DG_ShutdownNotepadRenderer() {
	if (g_renderer) {
		if (g_renderer->buffer) {
			free(g_renderer->buffer);
		}
		free(g_renderer);
		g_renderer = NULL;
	}
}
#endif
#endif

// ---------------- SOBEL GRAD-INTENSITY COLORING ----------------
// https://claude.ai/chat/a13f663a-2946-4c0a-8d62-e25f1ecc16af
// ---------------------------------------------------------------
// Used to determine if we should insert a color trigger
static int last_intensity = 0;
static int trigger_cooldown = 0;  // Prevent triggers too close together

// Maps brightness ranges to the color palette indices (0-15)
// Each entry represents the upper bound of that color's brightness range
static const uint8_t BRIGHTNESS_THRESHOLDS[] = {
	15,   // Black (7-8)
	31,   // Dark red (23-24)
	47,   // Red (39-40)
	63,   // Light red (55-56)
	79,   // Brown (71-72)
	95,   // Darker brown (87-88)
	111,  // Light brown (103-104)
	127,  // Green (119-120)
	143,  // Dark olive (133-134)
	159,  // Olive (151-152)
	175,  // Dark goldenrod (167-168)
	191,  // Red again (183-184)
	207,  // Blue (199-200)
	223,  // Yellow (215-216)
	239,  // Bright yellow (231-232)
	255   // Goldenrod (247-248)
};

// Optimized trigger mappings
static const struct 
{
	char trigger[4];  // Actual chars to write
	uint8_t length;   // Length of the trigger
} 


#define MD_TRIG
//#define XML_TRIG
//#define TCL_TRIG

#ifdef XML_TRIG
LANG_TRIGGERS[] =
{
	{{'<','<',0,0}, 2},  // Black
	{{'<','<',0,0}, 2},  // Dark red
	{{'<','<',0,0}, 2},  // Red
	{{'<','<',0,0}, 2},  // Light red
	{{'(',0,0,0}, 1},    // Brown
	{{'(',0,0,0}, 1},    // Darker brown
	{{'(',0,0,0}, 1},    // Light brown
	{{'<','!','-','-'}, 4}, // Green
	{{'<','!','-','-'}, 4}, // Dark olive
	{{'<','!','-','-'}, 4}, // Olive
	{{'<','!','-','-'}, 4}, // Dark goldenrod
	{{'<','<',0,0}, 2},  // Red again
	{{'<',0,0,0}, 1},    // Blue
	{{'[',0,0,0}, 1},    // Yellow
	{{'[',0,0,0}, 1},    // Bright yellow
	{{'[',0,0,0}, 1}     // Goldenrod
};
#endif

#ifdef TCL_TRIG
LANG_TRIGGERS[] = {
	{{'#',0,0,0}, 1},    // Black
	{{';',0,0,0}, 1},    // Dark red
	{{';',0,0,0}, 1},    // Red
	{{';',0,0,0}, 1},    // Light red
	{{'[',0,0,0}, 1},    // Brown
	{{'[',0,0,0}, 1},    // Darker brown
	{{'[',0,0,0}, 1},    // Light brown
	{{'#','#',0,0}, 2},  // Green (TCL comment)
	{{'#','#',0,0}, 2},  // Dark olive
	{{'#','#',0,0}, 2},  // Olive
	{{'#','#',0,0}, 2},  // Dark goldenrod
	{{';',0,0,0}, 1},    // Red again
	{{'$',0,0,0}, 1},    // Blue (variable reference)
	{{'{',0,0,0}, 1},    // Yellow
	{{'{',0,0,0}, 1},    // Bright yellow
	{{'{',0,0,0}, 1}     // Goldenrod
};
#endif

/*#ifdef MD_TRIG // Olde and breakey -- not made for NP++'s markdown._preinstalled.udl.xml
LANG_TRIGGERS[] = {
	{{'`',0,0,0}, 1},    // Black (inline code)
	{{'*',0,0,0}, 1},    // Dark red (emphasis)
	{{'*','*',0,0}, 2},  // Red (strong emphasis)
	{{'_',0,0,0}, 1},    // Light red (alt emphasis)
	{{'#',0,0,0}, 1},    // Brown (h1)
	{{'#','#',0,0}, 2},  // Darker brown (h2)
	{{'#','#','#',0}, 3},// Light brown (h3)
	{{'[',0,0,0}, 1},    // Green (link start)
	{{'!',0,0,0}, 1},    // Dark olive (image)
	{{'>',0,0,0}, 1},    // Olive (blockquote)
	{{'~',0,0,0}, 1},    // Dark goldenrod (strikethrough start)
	{{'*','*',0,0}, 2},  // Red again (strong emphasis)
	{{'`','`','`',0}, 3},// Blue (code block)
	{{'-',0,0,0}, 1},    // Yellow (list item)
	{{'=',0,0,0}, 1},    // Bright yellow (h1 underline)
	{{'|',0,0,0}, 1}     // Goldenrod (table separator)
};
#endif*/
/*#ifdef MD_TRIG
LANG_TRIGGERS[] = {
	{{'`',0,0,0}, 1},    // Black (inline code)
	{{'*',0,0,0}, 1},    // Dark red (emphasis)
	{{'*','*',0,0}, 2},  // Red (strong emphasis)
	{{'_',0,0,0}, 1},    // Light red (alt emphasis)
	{{'#',0,0,0}, 1},    // Brown (h1)
	{{'#','#',0,0}, 2},  // Darker brown (h2)
	{{'#','#','#',0}, 3},// Light brown (h3)
	{{'[',0,0,0}, 1},    // Green (link start)
	{{'!','[',0,0}, 2},  // Dark olive (image)
	{{'>',0,0,0}, 1},    // Olive (blockquote)
	{{'~','~',0,0}, 2},  // Dark goldenrod (strikethrough)
	{{'*','*','*',0}, 3},// Red again (strong emphasis alt)
	{{'`','`','`',0}, 3},// Blue (code block)
	{{'-',0,0,0}, 1},    // Yellow (list item)
	{{'=',0,0,0}, 1},    // Bright yellow (header underline)
	{{'|',0,0,0}, 1}     // Goldenrod (table separator)
};
#endif*/

/*#ifdef MD_TRIG // Exactly 2 char per line to prevent color issues
LANG_TRIGGERS[] = {
	{{'`','`',0,0}, 2},    // Black (using double chars to force state)
	{{'*','>',0,0}, 2},    // Dark red (combining markers)
	{{'#','*',0,0}, 2},    // Red 
	{{'_','_',0,0}, 2},    // Light red
	{{'[',']',0,0}, 2},    // Brown
	{{'<','>',0,0}, 2},    // Darker brown
	{{'{','}',0,0}, 2},    // Light brown
	{{'|','-',0,0}, 2},    // Green
	{{'=','=',0,0}, 2},    // Dark olive
	{{'+','+',0,0}, 2},    // Olive
	{{'~','~',0,0}, 2},    // Dark goldenrod
	{{'*','|',0,0}, 2},    // Red again
	{{'`','|',0,0}, 2},    // Blue
	{{'-','=',0,0}, 2},    // Yellow
	{{'=','>',0,0}, 2},    // Bright yellow
	{{'|','=',0,0}, 2}     // Goldenrod
};
#endif*/
#ifdef MD_TRIG
LANG_TRIGGERS[] = {
	{{'-','\\',0,0}, 2},    // Black (dash-backslash combo)
	{{'\\','/',0,0}, 2},    // Dark red (backslash-forward slash)  
	{{'/','-',0,0}, 2},     // Red (forward slash-dash)
	{{'-','/',0,0}, 2},     // Light red (dash-forward slash)
	{{'\\','-',0,0}, 2},    // Brown (backslash-dash)
	{{'/','\\',0,0}, 2},    // Darker brown (forward slash-backslash)
	//{{'~','-',0,0}, 2},     // Light brown (tilde-dash)
	{{'=','-',0,0}, 2},     // Green (equals-dash)
	{{':','-',0,0}, 2},     // Dark olive (colon-dash)
	{{'+','-',0,0}, 2},     // Olive (plus-dash)
	{{'.','-',0,0}, 2},     // Dark goldenrod (dot-dash)
	{{'-','.',0,0}, 2},     // Red again (dash-dot)
	{{'-','~',0,0}, 2},     // Blue (dash-tilde)
	{{'-','=',0,0}, 2},     // Yellow (dash-equals)
	{{'=','/',0,0}, 2},     // Bright yellow (equals-forward slash)
	{{'/',':',0,0}, 2}      // Goldenrod (forward slash-colon)
};
#endif
// Fast brightness to palette index mapping
// BRIGHTNESS IS A COLOR CODE FOR DOOM (for it's 8 bit color pallete)
static inline uint8_t brightness_to_palette_idx(uint8_t brightness) 
{
	for (uint8_t i = 0; i < sizeof(BRIGHTNESS_THRESHOLDS); i++) 
	{
		if (brightness <= BRIGHTNESS_THRESHOLDS[i])
			return i;
	}
	return sizeof(BRIGHTNESS_THRESHOLDS) - 1;
}

// Optimized trigger fill function
static inline uint8_t fill_mapped_trigger(char* buf, uint8_t brightness, uint8_t col_index)
{
	uint8_t palette_idx = brightness_to_palette_idx(brightness);
	uint8_t trigger_len = LANG_TRIGGERS[palette_idx].length;

	// Get outta here prevent buffer overrun
	if (col_index + trigger_len >= DOOMGENERIC_RESX)
		return 0;

	const char* trigger = LANG_TRIGGERS[palette_idx].trigger;

	// Direct memory copy of the exact number of bytes needed
	for (uint8_t i = 0; i < trigger_len; i++)
	{
		buf[i] = trigger[i];
	}

	return trigger_len;
}


// Edge intensity thresholds for color changes
#define LOW_EDGE 30
#define MID_EDGE 60
#define HIGH_EDGE 90


int sobel_operator(int x, int y, uint32_t* pixels, int width, int height) {
	// Sobel kernels
	int Gx[3][3] = {
		{-1, 0, 1},
		{-2, 0, 2},
		{-1, 0, 1}
	};
	int Gy[3][3] = {
		{ 1, 2, 1},
		{ 0, 0, 0},
		{-1,-2,-1}
	};

	int gx = 0, gy = 0;

	// Iterate over the 3x3 neighborhood
	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			int nx = x + i;
			int ny = y + j;

			// Ensure pixel is within bounds
			if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
				uint32_t pixel = pixels[ny * width + nx];
				int intensity = (pixel & 0xFF) + ((pixel >> 8) & 0xFF) + ((pixel >> 16) & 0xFF);
				intensity /= 3; // Grayscale intensity

				gx += Gx[i + 1][j + 1] * intensity;
				gy += Gy[i + 1][j + 1] * intensity;
			}
		}
	}

	// Compute gradient magnitude
	return sqrt(gx * gx + gy * gy);
}

// Add these globals
static float ema_fps = 60.0f;  // Initial estimate
static LARGE_INTEGER last_clear_time = { 0 };
static LARGE_INTEGER last_frame_time = { 0 };
static LARGE_INTEGER frequency = { 0 };

// Replace your fixed frame clearing with this adaptive version
void adaptive_frame_clear(void) {
	LARGE_INTEGER current_time;
	QueryPerformanceCounter(&current_time);

	// Calculate time since last frame
	float delta_sec = (float)(current_time.QuadPart - last_frame_time.QuadPart) / frequency.QuadPart;

	// Update EMA of FPS (alpha = 0.1 for ~3 sec window)
	float instant_fps = 1.0f / delta_sec;
	ema_fps = (0.1f * instant_fps) + (0.9f * ema_fps);

	// Calculate optimal frames between clears based on FPS
	// Target ~1 second between clears, but adjust if FPS is unstable
	int frames_between_clears = (int)(ema_fps + 0.5f);

	// Clamp to reasonable ranges
	frames_between_clears = max(30, min(120, frames_between_clears));

	// Check if we should clear based on adaptive frame count
	if (frame_count % frames_between_clears == 0) {
		// Check if at least 0.75 seconds have passed since last clear
		float time_since_clear = (float)(current_time.QuadPart - last_clear_time.QuadPart) / frequency.QuadPart;

		if (time_since_clear >= 0.75f) {
			// Clear and paste
			simulate_key_combo(VK_CONTROL, 'A');
			Sleep(1);
			simulate_key_combo(VK_CONTROL, 'V');

			// Update last clear time
			last_clear_time = current_time;
		}
	}

	// Update last frame time
	last_frame_time = current_time;
}

// DG_Render, DG_Frame, DG_Update, RenderFrames
void DG_DrawFrame()
{
	// Clear screen every frame in Windows
#ifdef OS_WINDOWS
	// printf("Clear\n");
	// system("cls");  // Add this
#endif

	uint32_t current_time = DG_GetTicksMs();
	uint32_t frame_time = current_time - last_time;

#ifdef DISP_NOTEPAD_PP
	// Rate limiting for Notepad++ updates
	DWORD current_tick = GetTickCount();
	if (current_tick - last_time < TARGET_FRAMETIME) {
		Sleep(1);  // Give some time back to the system
		return;
	}
#endif

	// Sleep the ms to make const fps
	if (frame_time < TARGET_FRAME_TIME)
	{
		Sleep(TARGET_FRAME_TIME - frame_time);
	}

	// Update frame timers
	frame_count++;
	last_time = current_time;

	// NEW: Handle "pause frame draw"
	if (!doUpdate) {
		if (wasPaused == FALSE)
		{
			// Good to clear the entire prev 60 frames!
			if (frame_count % 30 == 0) {
				// Clear all
				simulate_key_combo(VK_CONTROL, 'A');
				Sleep(1);
				simulate_key_combo(VK_CONTROL, 'V');
			}

			// Good to clear the entire prev ?? frames!
			// adaptive_frame_clear();

			Sleep(1);
			paste_to_active_window();
			memset(output_buffer, '\0', 1);
			paste_to_active_window();
		}

		wasPaused = TRUE;
		
		return;
		if (frame_count % fps_log_interval == 0) {
			printf("PAUSE SKIP");
		}

		return;
	}

	if (wasPaused) {  
		// Reset when resuming from pause state...
		sum_frame_time = 0;
		last_intensity = 0;
		wasPaused = FALSE;
	}
	else {
		// Continue draw!
		sum_frame_time += frame_time;
	}


	if (frame_count % fps_log_interval == 0) {
		uint32_t avg_frame_time = sum_frame_time / fps_log_interval;
		uint32_t fps = avg_frame_time > 0 ? 1000 / avg_frame_time : 0;
		printf("Frame-time: %d ms\n", avg_frame_time);
		printf("Framerate: %d FPS\n", fps);
		sum_frame_time = 0; // Reset accumulator
	}

	/* Clear screen if first frame */
	static int first_frame = 0;
	if (first_frame == 1) {
		first_frame = 0;
		CALL_STDOUT(fputs("\033[1;1H\033[2J", stdout), "DG_DrawFrame: fputs error %d");
	}

	/* fill output buffer */
#ifdef USE_COLOR
	uint32_t color = 0xFFFFFF00;
#endif
	unsigned row, col;
	struct color_t* in_pixel = (struct color_t*)DG_ScreenBuffer;
	struct color_t** videoPixel = (struct color_t**)I_VideoBuffer; // If the image is scaled down in DG_ScreenBuffer, then use the original source video buffer

#ifdef DISP_NOTEPAD_PP
	// Use renderer's buffer for Notepad++
	char* buf = g_renderer->buffer;
#else
	// Use regular output buffer for other modes
	char* buf = output_buffer;
#endif

#ifdef  DISP_CLIPBOARD
	// Bunch of NewLines, before frame data for Notepad autoscroll
	for (int i = 0; i < DOOMGENERIC_RESY; i++) {
		*buf++ = '\n';
	}
#endif // DISP_CLIPBOARD
	trigger_cooldown = 0; // For XML-color trigger ascii sequences min-gap
	last_intensity = 0;

	// LOOP THRU ALL ROWS 
	for (row = 0; row < DOOMGENERIC_RESY; row++)
	{
		// Track how many buffer positions we'll advance
		uint8_t chars_written = 0;

		// FOR EVERY ROW, DRAW HORIZONTALLY AT REX-X
		for (col = 0; col < DOOMGENERIC_RESX && chars_written < DOOMGENERIC_RESX * 2 - 1; col++)
		{
#ifdef HALF_SCALE
			int edge_intensity = sobel_operator(col * 2, row * 2, (uint32_t*)videoPixel, SCREENHEIGHT, SCREENWIDTH);
#else
			int edge_intensity = sobel_operator(col, row, (uint32_t*)videoPixel, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
#endif 
			//#ifdef USE_COLOR ... #endif (see commit 8533f067a2b45b and prior)

			uint8_t brightness = (in_pixel->r + in_pixel->g + in_pixel->b) / 3;
			
			// In your main rendering loop
			if (trigger_cooldown <= 0)
			{
				int edge_diff = edge_intensity - last_intensity;
				last_intensity = edge_intensity;
				if (edge_diff > 5 || edge_diff < -5)
				{
					uint8_t written = fill_mapped_trigger(buf, brightness, col, row);
					buf += written;
					chars_written += written;
					// Shorter cooldown to allow more color variation
					trigger_cooldown = 3; // SHORTENED FROM 5 
				}
			}
			
			// Else, the cooldown was not set and we need to make more chars to finish the row
			if(trigger_cooldown < 3)
			{
				if (trigger_cooldown > 0)
					trigger_cooldown--;

				// Get the char that corresponds to the brightness
				char v_char = grad[(brightness * grad_len) / 256];

				// Write 2 chars for aspect ratio correction
				*buf++ = v_char;
				*buf++ = v_char;
				
				// Count our two ASCII chars4
				chars_written += 2;  
			}

			// Advance pixel pointer only once per actual pixel
			in_pixel++;

			// Optional: Debug check for buffer overrun
#ifdef DEBUG
			if (chars_written > 6) {  // Max should be 4 (trigger) + 2 (ASCII)
				printf("Warning: Wrote %d chars for pixel at %d,%d\n", chars_written, col, row);
			}
#endif
		}
		*buf++ = '\n';
	}

	// Reset terminal colors to default
#ifdef USE_COLOR
	* buf++ = '\033';
	*buf++ = '[';
	*buf++ = '0';
	*buf = 'm';
#endif

#ifdef DISP_CLIPBOARD
	// Do the ascii copy-paste action
	copy_to_clipboard(output_buffer);
	Sleep(1);
	// Skip paste if not focused on target window...
	if (wcscmp(currWindowStr, gameWindowStr) != 0)
	{
		if (isFocused == TRUE) {
			wprintf("\n\nUNFOCUSED FROM GAME WINDOW!\n\n Game paused...\nWindow: %ls\n", currWindowStr);
		}

		// Don't update doom until game-window re-focused!
		Sleep(500);
		return;
	}
	// Good to paste!
	if (frame_count % 60 == 0) {
		// Clear all
		simulate_key_combo(VK_CONTROL, 'A');
		Sleep(1);
		simulate_key_combo(VK_CONTROL, 'V');
	}
	Sleep(1);
	paste_to_active_window();
	memset(output_buffer, '\0', buf - output_buffer + 1u);
	paste_to_active_window();
	// simulate_key_combo(VK_CONTROL, 'A'); Sleep(1); simulate_key_combo(VK_CONTROL, 'V');
#elif defined(DISP_NOTEPAD_PP)
	*buf = '\0';
	// Send to Notepad++ with error checking
	if (g_renderer && g_renderer->scintilla && IsWindow(g_renderer->scintilla)) {
		// Use SendMessageTimeout instead of SendMessage
		DWORD_PTR result;
		if (SendMessageTimeout(g_renderer->scintilla,
			SCI_SETTEXT,
			0,
			(LPARAM)g_renderer->buffer,
			SMTO_ABORTIFHUNG | SMTO_NORMAL,
			10000,  // 1 second timeout
			&result) == 0) {
			printf("\nFailed to send message to Notepad++\n");
			printf("-----------------------------------\n\n");
			printf(GetLastError());
		}
		else
		{
			printf("\nSent message to Notepad++!!\n");
		}
	}
	else
	{
		printf("\nNotepad++ window or Scintilla component not found\n");
	}

	// Clear buffer
	if (g_renderer && g_renderer->buffer)
	{
		memset(g_renderer->buffer, '\0', buf - g_renderer->buffer + 1u);
	}
#else // TERMINAL
	/* move cursor to top left corner and set bold text*/
	CALL_STDOUT(fputs("\033[;H\033[1m", stdout), "DG_DrawFrame: fputs error %d");
	/* flush output buffer */
	CALL_STDOUT(fputs(output_buffer, stdout), "DG_DrawFrame: fputs error %d");
	memset(output_buffer, '\0', buf - output_buffer + 1u);
#endif
}

void DG_SleepMs(const uint32_t ms)
{
#ifdef OS_WINDOWS
	Sleep(ms);
#else
	const struct timespec ts = (struct timespec){
		.tv_sec = ms / 1000,
		.tv_nsec = (ms % 1000ul) * 1000000,
	};
	nanosleep(&ts, NULL);
#endif
}

uint32_t DG_GetTicksMs()
{
	struct timespec ts;
	clock_gettime(CLK, &ts);

	return (ts.tv_sec - ts_init.tv_sec) * 1000 + (ts.tv_nsec - ts_init.tv_nsec) / 1000000;
}

#ifdef OS_WINDOWS
static inline unsigned char convertToDoomKey(const WORD wVirtualKeyCode, const CHAR AsciiChar)
{
	switch (wVirtualKeyCode) {
	case VK_RETURN:
		return KEY_ENTER;
	case VK_LEFT:
		return KEY_LEFTARROW;
	case VK_UP:
		return KEY_UPARROW;
	case VK_RIGHT:
		return KEY_RIGHTARROW;
	case VK_DOWN:
		return KEY_DOWNARROW;
	case VK_TAB:
		return KEY_TAB;
	case VK_F1:
		return KEY_F1;
	case VK_F2:
		return KEY_F2;
	case VK_F3:
		return KEY_F3;
	case VK_F4:
		return KEY_F4;
	case VK_F5:
		return KEY_F5;
	case VK_F6:
		return KEY_F6;
	case VK_F7:
		return KEY_F7;
	case VK_F8:
		return KEY_F8;
	case VK_F9:
		return KEY_F9;
	case VK_F10:
		return KEY_F10;
	case VK_F11:
		return KEY_F11;
	case VK_F12:
		return KEY_F12;
	case VK_BACK:
		return KEY_BACKSPACE;
	case VK_PAUSE:
		return KEY_PAUSE;
	case VK_RSHIFT:
		return KEY_RSHIFT;
	case VK_RCONTROL:
		return KEY_RCTRL;
	case VK_CAPITAL:
		return KEY_CAPSLOCK;
	case VK_NUMLOCK:
		return KEY_NUMLOCK;
	case VK_SCROLL:
		return KEY_SCRLCK;
	case VK_SNAPSHOT:
		return KEY_PRTSCR;
	case VK_HOME:
		return KEY_HOME;
	case VK_END:
		return KEY_END;
	case VK_PRIOR:
		return KEY_PGUP;
	case VK_NEXT:
		return KEY_PGDN;
	case VK_INSERT:
		return KEY_INS;
	case VK_DELETE:
		return KEY_DEL;
	case VK_NUMPAD0:
		return KEYP_0;
	case VK_NUMPAD1:
		return KEYP_1;
	case VK_NUMPAD2:
		return KEYP_2;
	case VK_NUMPAD3:
		return KEYP_3;
	case VK_NUMPAD4:
		return KEYP_4;
	case VK_NUMPAD5:
		return KEYP_5;
	case VK_NUMPAD6:
		return KEYP_6;
	case VK_NUMPAD7:
		return KEYP_7;
	case VK_NUMPAD8:
		return KEYP_8;
	case VK_NUMPAD9:
		return KEYP_9;
	default:
		return tolower(AsciiChar);
	}
}

// RB but probs use DWORD
static inline unsigned char convertToDoomKey_char(char wVirtualKeyCode)
{
	switch (wVirtualKeyCode) {
	case VK_RETURN:
		return KEY_ENTER;
	case VK_LEFT:
		return KEY_LEFTARROW;
	case VK_UP:
		return KEY_UPARROW;
	case VK_RIGHT:
		return KEY_RIGHTARROW;
	case VK_DOWN:
		return KEY_DOWNARROW;
	case VK_TAB:
		return KEY_TAB;
	case VK_F1:
		return KEY_F1;
	case VK_F2:
		return KEY_F2;
	case VK_F3:
		return KEY_F3;
	case VK_F4:
		return KEY_F4;
	case VK_F5:
		return KEY_F5;
	case VK_F6:
		return KEY_F6;
	case VK_F7:
		return KEY_F7;
	case VK_F8:
		return KEY_F8;
	case VK_F9:
		return KEY_F9;
	case VK_F10:
		return KEY_F10;
	case VK_F11:
		return KEY_F11;
	case VK_F12:
		return KEY_F12;
	case VK_BACK:
		return KEY_BACKSPACE;
	case VK_PAUSE:
		return KEY_PAUSE;
	case VK_RSHIFT:
		return KEY_RSHIFT;
	case VK_RCONTROL:
		return KEY_RCTRL;
	case VK_CAPITAL:
		return KEY_CAPSLOCK;
	case VK_NUMLOCK:
		return KEY_NUMLOCK;
	case VK_SCROLL:
		return KEY_SCRLCK;
	case VK_SNAPSHOT:
		return KEY_PRTSCR;
	case VK_HOME:
		return KEY_HOME;
	case VK_END:
		return KEY_END;
	case VK_PRIOR:
		return KEY_PGUP;
	case VK_NEXT:
		return KEY_PGDN;
	case VK_INSERT:
		return KEY_INS;
	case VK_DELETE:
		return KEY_DEL;
	case VK_NUMPAD0:
		return KEYP_0;
	case VK_NUMPAD1:
		return KEYP_1;
	case VK_NUMPAD2:
		return KEYP_2;
	case VK_NUMPAD3:
		return KEYP_3;
	case VK_NUMPAD4:
		return KEYP_4;
	case VK_NUMPAD5:
		return KEYP_5;
	case VK_NUMPAD6:
		return KEYP_6;
	case VK_NUMPAD7:
		return KEYP_7;
	case VK_NUMPAD8:
		return KEYP_8;
	case VK_NUMPAD9:
		return KEYP_9;
	default:
		return tolower("");
	}
}

#else
static unsigned char doomKeyIfTilda(const char** const buf, const unsigned char key)
{
	if (*((*buf) + 1) != '~')
		return '\0';
	(*buf)++;
	return key;
}

static inline unsigned char convertCsiToDoomKey(const char** const buf)
{
	switch (**buf) {
	case 'A':
		return KEY_UPARROW;
	case 'B':
		return KEY_DOWNARROW;
	case 'C':
		return KEY_RIGHTARROW;
	case 'D':
		return KEY_LEFTARROW;
	case 'H':
		return KEY_HOME;
	case 'F':
		return KEY_END;
	case '1':
		switch (*((*buf) + 1)) {
		case '5':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F5);
		case '7':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F6);
		case '8':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F7);
		case '9':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F8);
		default:
			return '\0';
		}
	case '2':
		switch (*((*buf) + 1)) {
		case '0':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F9);
		case '1':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F10);
		case '3':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F11);
		case '4':
			(*buf)++;
			return doomKeyIfTilda(buf, KEY_F12);
		case '~':
			(*buf)++;
			return KEY_INS;
		default:
			return '\0';
		}
	case '3':
		return doomKeyIfTilda(buf, KEY_DEL);
	case '5':
		return doomKeyIfTilda(buf, KEY_PGUP);
	case '6':
		return doomKeyIfTilda(buf, KEY_PGDN);
	default:
		return '\0';
	}
}

static inline unsigned char convertSs3ToDoomKey(const char** const buf)
{
	switch (**buf) {
	case 'P':
		return KEY_F1;
	case 'Q':
		return KEY_F2;
	case 'R':
		return KEY_F3;
	case 'S':
		return KEY_F4;
	default:
		return '\0';
	}
}

static inline unsigned char convertToDoomKey(const char** const buf)
{
	switch (**buf) {
	case '\012':
		return KEY_ENTER;
	case '\033':
		switch (*((*buf) + 1)) {
		case '[':
			*buf += 2;
			return convertCsiToDoomKey(buf);
		case 'O':
			*buf += 2;
			return convertSs3ToDoomKey(buf);
		default:
			return KEY_ESCAPE;
		}
	default:
		return tolower(**buf);
	}
}
#endif

// void DG_ReadInput(void)
// 
// static unsigned char prev_input_buffer[INPUT_BUFFER_LEN];
// 
// memcpy(prev_input_buffer, input_buffer, INPUT_BUFFER_LEN);
// memset(input_buffer, '\0', INPUT_BUFFER_LEN);
// memset(event_buffer, '\0', 2u * (size_t)EVENT_BUFFER_LEN);
// event_buf_loc = event_buffer;
// ifdef OS_WINDOWS
// const HANDLE hInputHandle = GetStdHandle(STD_INPUT_HANDLE);
// WINDOWS_CALL(hInputHandle == INVALID_HANDLE_VALUE, "DG_ReadInput: %s");
// 
// /* Disable canonical mode */
// DWORD old_mode, new_mode;
// WINDOWS_CALL(!GetConsoleMode(hInputHandle, &old_mode), "DG_ReadInput: %s");
// new_mode = old_mode;
// new_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
// WINDOWS_CALL(!SetConsoleMode(hInputHandle, new_mode), "DG_ReadInput: %s");
// 
// DWORD event_cnt;
// WINDOWS_CALL(!GetNumberOfConsoleInputEvents(hInputHandle, &event_cnt), "DG_ReadInput: %s");
// 
// /* ReadConsole is blocking so must manually process events */
// unsigned input_count = 0;
// if (event_cnt) {
// 	INPUT_RECORD input_records[32];
// 	WINDOWS_CALL(!ReadConsoleInput(hInputHandle, input_records, 32, &event_cnt), "DG_ReadInput: %s");
// 
// 	DWORD i;
// 	for (i = 0; i < event_cnt; i++) {
// 		if (input_records[i].Event.KeyEvent.bKeyDown && input_records[i].EventType == KEY_EVENT) {
// 			unsigned char inp = convertToDoomKey(input_records[i].Event.KeyEvent.wVirtualKeyCode, input_records[i].Event.KeyEvent.uChar.AsciiChar);
// 			inp = pressedKey;
// 			if (inp) {
// 				input_buffer[input_count++] = inp;
// 				if (input_count == INPUT_BUFFER_LEN - 1u)
// 					break;
// 			}
// 		}
// 	}
// }
// 
// 
// WINDOWS_CALL(!SetConsoleMode(hInputHandle, old_mode), "DG_ReadInput: %s");
// else /* defined(OS_WINDOWS) */
// static char raw_input_buffer[INPUT_BUFFER_LEN];
// struct termios oldt, newt;
// 
// memset(raw_input_buffer, '\0', INPUT_BUFFER_LEN);
// 
// /* Disable canonical mode */
// CALL(tcgetattr(STDIN_FILENO, &oldt), "DG_DrawFrame: tcgetattr error %d");
// newt = oldt;
// newt.c_lflag &= ~(ICANON);
// newt.c_cc[VMIN] = 0;
// newt.c_cc[VTIME] = 0;
// CALL(tcsetattr(STDIN_FILENO, TCSANOW, &newt), "DG_DrawFrame: tcsetattr error %d");
// 
// CALL(read(2, raw_input_buffer, INPUT_BUFFER_LEN - 1u) < 0, "DG_DrawFrame: read error %d");
// 
// CALL(tcsetattr(STDIN_FILENO, TCSANOW, &oldt), "DG_DrawFrame: tcsetattr error %d");
// 
// /* Flush input buffer to prevent read of previous unread input */
// CALL(tcflush(STDIN_FILENO, TCIFLUSH), "DG_DrawFrame: tcflush error %d");
// 
// /* create input buffer */
// const char *raw_input_buf_loc = raw_input_buffer;
// unsigned char *input_buf_loc = input_buffer;
// while (*raw_input_buf_loc) {
// 	const unsigned char inp = convertToDoomKey(&raw_input_buf_loc);
// 	if (!inp)
// 		break;
// 	*input_buf_loc++ = inp;
// 	raw_input_buf_loc++;
// }
// endif
// /* construct event array */
// int i, j;
// for (i = 0; input_buffer[i]; i++) {
// 	/* skip duplicates */
// 	for (j = i + 1; input_buffer[j]; j++) {
// 		if (input_buffer[i] == input_buffer[j])
// 			goto LBL_CONTINUE_1;
// 	}
// 
// 	/* pressed events */
// 	for (j = 0; prev_input_buffer[j]; j++) {
// 		if (input_buffer[i] == prev_input_buffer[j])
// 			goto LBL_CONTINUE_1;
// 	}
// 	*event_buf_loc++ = 0x0100 | input_buffer[i];
// 
// LBL_CONTINUE_1:;
// }
// 
// /* depressed events */
// for (i = 0; prev_input_buffer[i]; i++) {
// 	for (j = 0; input_buffer[j]; j++) {
// 		if (prev_input_buffer[i] == input_buffer[j])
// 			goto LBL_CONTINUE_2;
// 	}
// 	*event_buf_loc++ = 0xFF & prev_input_buffer[i];
// 
// LBL_CONTINUE_2:;
// }
// 
// event_buf_loc = event_buffer;
// 

int DG_GetKey(int* const pressed, unsigned char* const doomKey)
{
	if (event_buf_loc == NULL || *event_buf_loc == 0)
		return 0;

	*pressed = *event_buf_loc >> 8;
	*doomKey = *event_buf_loc & 0xFF;
	event_buf_loc++;
	return 1;
}

void DG_SetWindowTitle(const char* const title)
{
	CALL_STDOUT(fputs("\033]2;", stdout), "DG_SetWindowTitle: fputs error %d");
	CALL_STDOUT(fputs(title, stdout), "DG_SetWindowTitle: fputs error %d");
	CALL_STDOUT(fputs("\033\\", stdout), "DG_SetWindowTitle: fputs error %d");
}

// =========================  GLOBAL INPUT HANDLING  =========================
// Global state for input handling
// Add to InputState struct
typedef struct {
	HHOOK keyboardHook;
	CRITICAL_SECTION inputLock;
	unsigned char current_input_buffer[INPUT_BUFFER_LEN];
	unsigned input_count;
	BOOL keyStates[256];  // Track key states to handle auto-repeat
} InputState;

static InputState g_inputState = { 0 };
static unsigned char input_buffer[INPUT_BUFFER_LEN];
static unsigned short event_buffer[EVENT_BUFFER_LEN];
static unsigned short* event_buf_loc;

// Modified keyboard hook callback
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode != HC_ACTION)
		return CallNextHookEx(g_inputState.keyboardHook, nCode, wParam, lParam);

	KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;

	// Filter out injected keystrokes
	if (pKeyBoard->flags & LLKHF_INJECTED)
		return CallNextHookEx(g_inputState.keyboardHook, nCode, wParam, lParam);

	EnterCriticalSection(&g_inputState.inputLock);

	// Process key-up events first to prevent sticking
	if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
	{
		// Always clear the key state first
		g_inputState.keyStates[pKeyBoard->vkCode] = FALSE;

		// Get the corresponding game input
		BYTE state[256] = { 0 };
		WORD ascii = 0;

		ToAscii(pKeyBoard->vkCode, pKeyBoard->scanCode, state, &ascii, 0);
		unsigned char inp = convertToDoomKey(pKeyBoard->vkCode, (char)ascii);

		// Map special keys
		if (pKeyBoard->vkCode == 'R' || pKeyBoard->vkCode == VK_SPACE ||
			pKeyBoard->vkCode == VK_CONTROL || pKeyBoard->vkCode == VK_LCONTROL ||
			pKeyBoard->vkCode == VK_RCONTROL)
		{
			inp = KEY_FIRE;
		}
		else if (pKeyBoard->vkCode == 'E') inp = KEY_USE;
		else if (pKeyBoard->vkCode == 'A') inp = KEY_STRAFE_L;
		else if (pKeyBoard->vkCode == 'D') inp = KEY_STRAFE_R;
		else if (pKeyBoard->vkCode == 'P') inp = KEY_ESCAPE;

		// Remove all instances of the key from buffer
		if (inp)
		{
			unsigned i = 0;
			while (i < g_inputState.input_count)
			{
				if (g_inputState.current_input_buffer[i] == inp)
				{
					memmove(&g_inputState.current_input_buffer[i],
						&g_inputState.current_input_buffer[i + 1],
						g_inputState.input_count - i - 1);
					g_inputState.input_count--;
				}
				else
				{
					i++;
				}
			}
		}
	}
	// Process key-down events
	else if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) &&
		g_inputState.input_count < INPUT_BUFFER_LEN - 1)
	{
		BYTE state[256] = { 0 };
		WORD ascii = 0;
		GetKeyboardState(state);
		ToAscii(pKeyBoard->vkCode, pKeyBoard->scanCode, state, &ascii, 0);
		unsigned char inp = convertToDoomKey(pKeyBoard->vkCode, (char)ascii);

		// Handle game loop control
		if (doUpdate == FALSE && inp == key_doUpdate)
		{
			HWND curWin = GetWin();
			if (curWin != NULL)
			{
				GetWindowTextW(curWin, gameWindowStr, 30);
				doUpdate = TRUE;
				printf("\n\nStart key pressed!\n");
				printf("Running game on window %ls\n", gameWindowStr);
			}
			else
			{
				wprintf(L"No window open. Select a window in order to start game with key ']'\n");
			}
		}
		else if (doUpdate == TRUE && inp == key_dontUpdate)
		{
			doUpdate = FALSE;
			printf("\n\nStop key pressed! Pausing game\n\n");
		}

		// Map special keys
		if (pKeyBoard->vkCode == 'R' || pKeyBoard->vkCode == VK_SPACE ||
			pKeyBoard->vkCode == VK_CONTROL || pKeyBoard->vkCode == VK_LCONTROL ||
			pKeyBoard->vkCode == VK_RCONTROL)
		{
			inp = KEY_FIRE;
		}
		else if (pKeyBoard->vkCode == 'E') inp = KEY_USE;
		else if (pKeyBoard->vkCode == 'A') inp = KEY_STRAFE_L;
		else if (pKeyBoard->vkCode == 'D') inp = KEY_STRAFE_R;
		else if (pKeyBoard->vkCode == 'P') inp = KEY_ESCAPE;

		// Add key if it's valid and not already pressed
		if (inp && !g_inputState.keyStates[pKeyBoard->vkCode])
		{
			g_inputState.keyStates[pKeyBoard->vkCode] = TRUE;

			// Check if key is already in buffer
			BOOL keyExists = FALSE;
			for (unsigned i = 0; i < g_inputState.input_count; i++)
			{
				if (g_inputState.current_input_buffer[i] == inp)
				{
					keyExists = TRUE;
					break;
				}
			}

			// Add key if not already present
			if (keyExists == FALSE)
			{
				g_inputState.current_input_buffer[g_inputState.input_count++] = inp;
			}
		}
	}

	LeaveCriticalSection(&g_inputState.inputLock);
	return CallNextHookEx(g_inputState.keyboardHook, nCode, wParam, lParam);
}


// Modified initialization
BOOL InitializeInput(void)
{
	InitializeCriticalSection(&g_inputState.inputLock);
	memset(&g_inputState.current_input_buffer, 0, INPUT_BUFFER_LEN);
	memset(&g_inputState.keyStates, 0, sizeof(g_inputState.keyStates));
	g_inputState.input_count = 0;

	// Try to get debug privileges for the hook
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
		CloseHandle(hToken);
	}

	g_inputState.keyboardHook = SetWindowsHookEx(
		WH_KEYBOARD_LL,
		LowLevelKeyboardProc,
		GetModuleHandle(NULL),
		0
	);

	if (!g_inputState.keyboardHook)
	{
		printf("Failed to install keyboard hook: %d\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

// Add debug printing to DG_ReadInput (temporary)
void DG_ReadInput(void)
{
	static unsigned char prev_input_buffer[INPUT_BUFFER_LEN];

	memcpy(prev_input_buffer, input_buffer, INPUT_BUFFER_LEN);
	memset(input_buffer, '\0', INPUT_BUFFER_LEN);
	memset(event_buffer, '\0', 2u * (size_t)EVENT_BUFFER_LEN);
	event_buf_loc = event_buffer;

	EnterCriticalSection(&g_inputState.inputLock);

	// Copy current state to input buffer
	memcpy(input_buffer, g_inputState.current_input_buffer, g_inputState.input_count);
	input_buffer[g_inputState.input_count] = '\0';

	LeaveCriticalSection(&g_inputState.inputLock);

	// Construct event array
	int i, j;
	for (i = 0; input_buffer[i]; i++) {
		// Skip duplicates
		for (j = i + 1; input_buffer[j]; j++) {
			if (input_buffer[i] == input_buffer[j])
				goto LBL_CONTINUE_1;
		}
		// Pressed events
		for (j = 0; prev_input_buffer[j]; j++) {
			if (input_buffer[i] == prev_input_buffer[j])
				goto LBL_CONTINUE_1;
		}
		*event_buf_loc++ = 0x0100 | input_buffer[i];
	LBL_CONTINUE_1:;
	}

	// Depressed events
	for (i = 0; prev_input_buffer[i]; i++) {
		for (j = 0; input_buffer[j]; j++) {
			if (prev_input_buffer[i] == input_buffer[j])
				goto LBL_CONTINUE_2;
		}
		*event_buf_loc++ = 0xFF & prev_input_buffer[i];
	LBL_CONTINUE_2:;
	}

	event_buf_loc = event_buffer;
}


void CleanupInput(void)
{
	if (g_inputState.keyboardHook)
	{
		UnhookWindowsHookEx(g_inputState.keyboardHook);
		g_inputState.keyboardHook = NULL;
	}

	DeleteCriticalSection(&g_inputState.inputLock);
}
// ========================= (END) GLOBAL INPUT HANDLING  =========================


// Modified main function to handle Windows messages
int main(int argc, char** argv)
{
	printf("\n\nDOOM main\n\n");
	Sleep(1000);  // Sleep for a second to allow the console to initialize

#ifdef OS_WINDOWS
	if (!InitializeInput())
	{
		printf("\nFailed to initialize input system\n");
		return 1;
	}
#endif

#ifdef  DISP_CLIPBOARD
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&last_clear_time);
	QueryPerformanceCounter(&last_frame_time);

	if (!init_clear_hotkeys()) {
		printf("Failed to register hotkey\n");
		return 1;
	}

	// set TRIG_SEGMENT_LENGTHS
	// int len = sizeof(TRIG_SEGMENT_COLORS);
	// for (size_t i = 0; i < len; i++)
	// {
	// 	TRIG_SEGMENT_LENGTHS[i] = strlen(TRIG_SEGMENT_COLORS[i]);
	// }
	printf("*TODO* Copied len of each TRIG_SEGMENT_COLORS, which has %d\n color-triggers", 0); // , len);
#endif //  DISP_CLIPBOARD


#ifdef DISP_NOTEPAD_PP
	Sleep(1000);  // Sleep for a second to allow the console to initialize
	printf("\nInitialize Notepad++ renderer\n");
	if (!DG_InitNotepadRenderer()) {
		printf("Failed to initialize Notepad++ renderer\n");
		return 1;
	}
	printf("\nNotepad++ renderer initialized!!!\n");
	Sleep(1000);  // Sleep for a second to allow the console to initialize
#endif
	doomgeneric_Create(argc, argv);

#ifdef OS_WINDOWS
	MSG msg;

	while (TRUE)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				goto cleanup;
			if (msg.message == WM_HOTKEY)
				process_hotkey(msg.wParam);
		}

		// Await game-loop run/pause keystroke
#ifdef  DISP_CLIPBOARD
		// 1. Update current window reference
		HWND curWin = GetWin();  // Current window info
		if (curWin != NULL) {
			GetWindowTextW(curWin, currWindowStr, 30);	// Set The wide-char version of win name, outputted to curWinStr
		}

		if (doUpdate == FALSE)
		{
			// 2. Print status
			if (frame_count % 360 == 0) {
				wprintf(L"Awaiting hotkey ( ']' ) to Start DOOM..\nCurrent Window: %ls\n", currWindowStr);
			}
			// 3. Don't update doom until \ is pressed! 
			// Sleep(15);
			frame_count++;
		}
#endif 

		// Game update
		DG_ReadInput();  // Add this here if you can't modify doomgeneric_Tick
		doomgeneric_Tick();
	}

#ifdef DISP_NOTEPAD_PP
	printf("Shutdown Notepad++ renderer\n");
	DG_ShutdownNotepadRenderer();
#endif

cleanup:
	cleanup_clear_hotkeys();
	CleanupInput();
#else
	for (;;)
	{
		doomgeneric_Tick();
	}
#endif

	return 0;
}
