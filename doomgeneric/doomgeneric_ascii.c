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

static void winError(char *const format)
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
int clock_gettime(const int p, struct timespec *const spec)
{
	(void)p;
	__int64 wintime;
	GetSystemTimeAsFileTime((FILETIME *)&wintime);
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

static const char grad[] = "  __--<<\\/\\/~~##░░▒▒▓▓████████"; // static const char grad[] =  " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
static size_t grad_len;  // Excludes the null terminator, set in init
int frame_count = 0;

struct color_t {
	uint32_t b : 8;
	uint32_t g : 8;
	uint32_t r : 8;
	uint32_t a : 8;
};

static char *output_buffer;
static size_t output_buffer_size;
static struct timespec ts_init;

static unsigned char input_buffer[INPUT_BUFFER_LEN];
static uint16_t event_buffer[EVENT_BUFFER_LEN];
static uint16_t *event_buf_loc;

DWORD pressedKey; // RB for global KB hooks

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
	output_buffer = malloc(output_buffer_size);

	clock_gettime(CLK, &ts_init);

	memset(input_buffer, '\0', INPUT_BUFFER_LEN);
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


const int CLEAR_INTERVAL = 30;


// Function to copy text to the clipboard
void copy_to_clipboard(const char* text) 
{
	// Open the clipboard
	if (!OpenClipboard(NULL)) {
		fprintf(stderr, "Failed to open clipboard.\n");
		
		int numAttepts = 0;
		while (!OpenClipboard(NULL))
		{
			Sleep(1);
			if (numAttepts > 10)
				return;
		}
		// return;
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

static uint32_t sum_frame_time = 0;
static uint32_t last_time = 0;
const int fps_log_interval = 60; // frames
const int TARGET_FRAMETIME = 40;

void DG_DrawFrame()
{    
// Clear screen every frame in Windows
#ifdef OS_WINDOWS
	// printf("Clear\n");
	// system("cls");  // Add this
#endif
	
	uint32_t current_time = DG_GetTicksMs();
	uint32_t frame_time = current_time - last_time;
	last_time = current_time;


	// Sleep the ms to make const fps
	if (frame_time < TARGET_FRAMETIME)
	{
		Sleep(TARGET_FRAMETIME - frame_time);
	}

	sum_frame_time += frame_time;
	frame_count++;

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
	struct color_t *pixel = (struct color_t *)DG_ScreenBuffer;
	char *buf = output_buffer;

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
			*buf++ = v_char;	// Write a SECOND character, cause such chars are 2x taller than their width
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

	//Sleep(1);
	copy_to_clipboard(output_buffer);
	Sleep(1);
	simulate_key_combo(VK_CONTROL, 'A'); // Simulates Ctrl+A
	Sleep(1);
	simulate_key_combo(VK_CONTROL, 'V'); // Simulates Ctrl+V

	/* move cursor to top left corner and set bold text*/
	// CALL_STDOUT(fputs("\033[;H\033[1m", stdout), "DG_DrawFrame: fputs error %d");
	/* flush output buffer */
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
static unsigned char doomKeyIfTilda(const char **const buf, const unsigned char key)
{
	if (*((*buf) + 1) != '~')
		return '\0';
	(*buf)++;
	return key;
}

static inline unsigned char convertCsiToDoomKey(const char **const buf)
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

static inline unsigned char convertSs3ToDoomKey(const char **const buf)
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

static inline unsigned char convertToDoomKey(const char **const buf)
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

// oid DG_ReadInput(void)
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

int DG_GetKey(int *const pressed, unsigned char *const doomKey)
{
	if (event_buf_loc == NULL || *event_buf_loc == 0)
		return 0;

	*pressed = *event_buf_loc >> 8;
	*doomKey = *event_buf_loc & 0xFF;
	event_buf_loc++;
	return 1;
}

void DG_SetWindowTitle(const char *const title)
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
	if (nCode == HC_ACTION)
	{
		KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;

		// Filter out injected keystrokes
		if (pKeyBoard->flags & LLKHF_INJECTED)
			return CallNextHookEx(g_inputState.keyboardHook, nCode, wParam, lParam);

		EnterCriticalSection(&g_inputState.inputLock);

		// Only process if we have room in the buffer
		if (g_inputState.input_count < INPUT_BUFFER_LEN - 1u)
		{
			// Get both virtual key and scan code for better mapping
			BYTE state[256] = { 0 };
			WORD ascii = 0;
			GetKeyboardState(state);
			ToAscii(pKeyBoard->vkCode, pKeyBoard->scanCode, state, &ascii, 0);

			unsigned char inp = convertToDoomKey(pKeyBoard->vkCode, (char)ascii);

			// Only process if we have room in the buffer
			if (g_inputState.input_count < INPUT_BUFFER_LEN - 1u)
			{
				// Map both CTRL and Space to the shooting key
				// CTRL doesnt seem to get picked up, space does
				if (pKeyBoard->vkCode == VK_SPACE ||
					pKeyBoard->vkCode == VK_CONTROL ||
					pKeyBoard->vkCode == VK_LCONTROL ||
					pKeyBoard->vkCode == VK_RCONTROL)
				{
					inp = KEY_FIRE;  // or whatever DOOM's fire key constant is
				}
				else if (pKeyBoard->vkCode == 'E') {
					inp = KEY_USE;
				}
				else if (pKeyBoard->vkCode == 'A') {
					inp = KEY_STRAFE_L;
				}
				else if (pKeyBoard->vkCode == 'D') {
					inp = KEY_STRAFE_R;
				}
			}

			if (inp)
			{
				switch (wParam)
				{
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
					// Only process if key wasn't already down
					if (!g_inputState.keyStates[pKeyBoard->vkCode])
					{
						g_inputState.keyStates[pKeyBoard->vkCode] = TRUE;

						// Add key to current input buffer if not already present
						for (unsigned i = 0; i < g_inputState.input_count; i++)
						{
							if (g_inputState.current_input_buffer[i] == inp)
								goto skip_add;
						}
						g_inputState.current_input_buffer[g_inputState.input_count++] = inp;
					}
				skip_add:
					break;

				case WM_KEYUP:
				case WM_SYSKEYUP:
					g_inputState.keyStates[pKeyBoard->vkCode] = FALSE;
					// Remove key from current input buffer
					for (unsigned i = 0; i < g_inputState.input_count; i++)
					{
						if (g_inputState.current_input_buffer[i] == inp)
						{
							memmove(&g_inputState.current_input_buffer[i],
								&g_inputState.current_input_buffer[i + 1],
								g_inputState.input_count - i - 1);
							g_inputState.input_count--;
							break;
						}
					}
					break;
				}
			}
		}

		LeaveCriticalSection(&g_inputState.inputLock);
	}

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

	// Debug print the previous state
	printf("Previous buffer: ");
	for (int i = 0; prev_input_buffer[i]; i++) {
		printf("%d ", prev_input_buffer[i]);
	}
	printf("\n");

	memcpy(prev_input_buffer, input_buffer, INPUT_BUFFER_LEN);
	memset(input_buffer, '\0', INPUT_BUFFER_LEN);
	memset(event_buffer, '\0', 2u * (size_t)EVENT_BUFFER_LEN);
	event_buf_loc = event_buffer;

	EnterCriticalSection(&g_inputState.inputLock);

	// Copy current state to input buffer
	memcpy(input_buffer, g_inputState.current_input_buffer, g_inputState.input_count);
	input_buffer[g_inputState.input_count] = '\0';

	// Debug print the current state
	printf("Current buffer: ");
	for (int i = 0; input_buffer[i]; i++) {
		printf("%d ", input_buffer[i]);
	}
	printf("\n");

	LeaveCriticalSection(&g_inputState.inputLock);

	// Debug print event generation
	printf("Generating events...\n");

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
		printf("Generated press event: 0x%04X\n", 0x0100 | input_buffer[i]);
	LBL_CONTINUE_1:;
	}

	// Depressed events
	for (i = 0; prev_input_buffer[i]; i++) {
		for (j = 0; input_buffer[j]; j++) {
			if (prev_input_buffer[i] == input_buffer[j])
				goto LBL_CONTINUE_2;
		}
		*event_buf_loc++ = 0xFF & prev_input_buffer[i];
		printf("Generated release event: 0x%04X\n", 0xFF & prev_input_buffer[i]);
	LBL_CONTINUE_2:;
	}

	// Print final event buffer
	printf("Event buffer: ");
	for (unsigned short* p = event_buffer; *p; p++) {
		printf("0x%04X ", *p);
	}
	printf("\n");

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
#ifdef OS_WINDOWS
	if (!InitializeInput())
	{
		printf("Failed to initialize input system\n");
		return 1;
	}
#endif

	doomgeneric_Create(argc, argv);

#ifdef OS_WINDOWS
	MSG msg;
	while (TRUE)
	{
		// Process Windows messages to keep the hook alive
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				goto cleanup;
		}

		DG_ReadInput();  // Add this here if you can't modify doomgeneric_Tick
		doomgeneric_Tick();
	}

cleanup:
	CleanupInput();
#else
	for (;;)
	{
		doomgeneric_Tick();
	}
#endif

	return 0;
}