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

#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_system.h"

#include <ctype.h>
#include <emscripten.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32
#if defined(_WIN32) || defined(WIN32)

#define OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define FRAME_LIMITING
// #define DOUBLE_CHAR_ASPECT
// #define USE_COLOR

#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

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

#define GRAD_LEN 4u
#define INPUT_BUFFER_LEN 16u
#define EVENT_BUFFER_LEN ((INPUT_BUFFER_LEN)*2u - 1u)

static const char grad[] = "  __--<<\\/\\/~~##░░▒▒▓▓████████"; // static const char grad[] =  " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
static size_t grad_len;  // Excludes the null terminator, set in init
int frame_count = 0;

uint32_t start_time; // rb

struct color_t {
	uint32_t b : 8;
	uint32_t g : 8;
	uint32_t r : 8;
	uint32_t a : 8;
};

static char* output_buffer;
static size_t output_buffer_size;
static struct timespec ts_init;

static unsigned char input_buffer[INPUT_BUFFER_LEN];
static uint16_t event_buffer[EVENT_BUFFER_LEN];
static uint16_t* event_buf_loc;

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
	start_time = get_time();
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
	output_buffer = malloc(output_buffer_size);

	clock_gettime(CLK, &ts_init);

	memset(input_buffer, '\0', INPUT_BUFFER_LEN);
}

static uint32_t sum_frame_time = 0;
static uint32_t last_time = 0;
const int fps_log_interval = 60; // frames

void DG_DrawFrame()
{
	// Clear screen every frame in Windows
#ifdef OS_WINDOWS
	printf("Clear\n");
	system("cls");  // Add this
#endif

	uint32_t current_time = DG_GetTicksMs();
	uint32_t frame_time = current_time - last_time;
	last_time = current_time;

	sum_frame_time += frame_time;
	frame_count++;

	if (frame_count % fps_log_interval == 0) {
		uint32_t avg_frame_time = sum_frame_time / fps_log_interval;
		uint32_t fps = avg_frame_time > 0 ? 1000 / avg_frame_time : 0;
		printf("Frame-time: %d ms\n", avg_frame_time);
		printf("Framerate: %d FPS\n", fps);
		sum_frame_time = 0; // Reset accumulator
	}

	// Frame-rate Limiting
#ifdef FRAME_LIMITING
	if (frame_time < TARGET_FRAME_TIME) {
		return;
	}
#endif
	last_frame_time = current_time;

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
	struct color_t* pixel = (struct color_t*)DG_ScreenBuffer;
	char* buf = output_buffer;

	// RB added comments
	// Iterate through each row and column of the output resolution
	for (row = 0; row < DOOMGENERIC_RESY; row++)
	{
		for (col = 0; col < DOOMGENERIC_RESX; col++)
		{
#ifdef USE_COLOR
			// Check if color changed from previous pixel (comparing RGB, ignoring alpha)
			if ((color ^ *(uint32_t*)pixel) & 0x00FFFFFF) {
				// ANSI escape sequence for 24-bit RGB color:
				// \033[38;2;R;G;B;m
				*buf++ = '\033';  // Escape character
				*buf++ = '[';     // Start ANSI sequence
				*buf++ = '3';     // Foreground color
				*buf++ = '8';     // 24-bit RGB mode
				*buf++ = ';';
				*buf++ = '2';     // RGB submode
				*buf++ = ';';
				BYTE_TO_TEXT(buf, pixel->r);  // Convert R value to ASCII
				*buf++ = ';';
				BYTE_TO_TEXT(buf, pixel->g);  // Convert G value to ASCII
				*buf++ = ';';
				BYTE_TO_TEXT(buf, pixel->b);  // Convert B value to ASCII
				*buf++ = 'm';     // End sequence
				color = *(uint32_t*)pixel;   // Store current color
			}
#endif
			// Convert RGB to grayscale index into ASCII gradient array
			char v_char = grad[(pixel->r + pixel->g + pixel->b) * grad_len / 765u];  // 765 = 255*3
			*buf++ = v_char;	// Write the character
#ifdef DOUBLE_CHAR_ASPECT // RB: account for char-width
			*buf++ = v_char;	// Write a SECOND character, cause such chars are twice/double taller than their width
#endif
			pixel++;
		}
		*buf++ = '\n';           // End of row
	}

	// Reset terminal colors to default
#ifdef USE_COLOR
	*buf++ = '\033';
	*buf++ = '[';
	*buf++ = '0';
	*buf = 'm';
#endif

	// Combine input processing and frame update
	EM_ASM({
		// 1. Process input queue
		// clang-format off
		for (let key of Object.keys(pressed_keys)) {
			key_queue.push([key, !!pressed_keys[key]]);
			if (pressed_keys[key] == = 0)
				delete pressed_keys[key];
			if (pressed_keys[key] == = 2)
				pressed_keys[key] = 0;
			// clang-format on
	}

	// 2. Update ASCII frame
	update_ascii_frame($0, $1, $2, $3); }, output_buffer, output_buffer_size, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
}

	/* doom-ascii move cursor to top left corner and set bold text*/
	// CALL_STDOUT(fputs("\033[;H\033[1m", stdout), "DG_DrawFrame: fputs error %d
	/* doom-ascii flush output buffer */
	// CALL_STDOUT(fputs(output_buffer, stdout), "DG_DrawFrame: fputs error %d");

	/* clear output buffer */
	memset(output_buffer, '\0', buf - output_buffer + 1u);
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

// pdf-js implementation
uint32_t get_time()
{
	return EM_ASM_INT({
	  return Date.now();
		});
}

uint32_t DG_GetTicksMs()
{
	// pdf-js implementation
	return get_time() - start_time;
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

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
	int key_data = EM_ASM_INT({
		// clang-format off
		if (key_queue.length == = 0)
		  return 0;
		let key_data = key_queue.shift();
		let key = key_data[0];
		let pressed = key_data[1];
		return (pressed << 8) | key;
		// clang-format on
		});

	if (key_data == 0)
		return 0;

	*pressed = key_data >> 8;
	*doomKey = key_data & 0xFF;
	return 1;
}

// Called FROM pdf js to get codes on press
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

void DG_SetWindowTitle(const char* const title)
{
	// title = "dum.exe";
	CALL_STDOUT(fputs("\033]2;", stdout), "DG_SetWindowTitle: fputs error %d");
	CALL_STDOUT(fputs(title, stdout), "DG_SetWindowTitle: fputs error %d");
	CALL_STDOUT(fputs("\033\\", stdout), "DG_SetWindowTitle: fputs error %d");
}

// pdfjs implement
int main(int argc, char** argv)
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