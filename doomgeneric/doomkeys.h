//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
//       Key definitions
//

#ifndef __DOOMKEYS__
#define __DOOMKEYS__

// #define __RB_KEYS_OVERRIDE__ // RB Override keys. Distinguishes need for handling windows key-up/key-down events (keyup/keydown, up/down)... See "LRESULT CALLBACK LowLevelKeyboardProc(..)" in doomgeneric_ascii.c
#ifdef __RB_KEYS_OVERRIDE__ // RB START __RB_KEYS_OVERRIDE__
#include <winuser.rh>
//
// RB overriding move/interact keys for ASCII/clipboard 
// This eliminates need for manual conversion via KB-interrupt handles [see "LRESULT CALLBACK LowLevelKeyboardProc(..)"]
// Mapped to Windows VK codes for simplicity
//

// For DG_ReadInput in doomgeneric_ascii for pause/run (doUpdate/dontUpdate)
#define KEY_START_UPDATE        ']'
#define KEY_STOP_UPDATE			'['


#define VK_NUMPAD0        0x60
#define VK_NUMPAD1        0x61
#define VK_NUMPAD2        0x62
#define VK_NUMPAD3        0x63
#define VK_NUMPAD4        0x64
#define VK_NUMPAD5        0x65
#define VK_NUMPAD6        0x66
#define VK_NUMPAD7        0x67
#define VK_NUMPAD8        0x68
#define VK_NUMPAD9        0x69

// Movement
#define KEY_RIGHTARROW  VK_NUMPAD6   // Turn right
#define KEY_LEFTARROW   VK_NUMPAD4   // Turn left  
#define KEY_UPARROW     VK_NUMPAD8   // Move forward
#define KEY_DOWNARROW   VK_NUMPAD5   // Move backward
#define KEY_STRAFE_L    VK_NUMPAD1   // Strafe left
#define KEY_STRAFE_R    VK_NUMPAD3   // Strafe right

// Control
#define KEY_USE         'E'          
#define KEY_FIRE        VK_SPACE     
#define KEY_ESCAPE      'P'          

#else // RB END __RB_KEYS_OVERRIDE__
//
// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
// Most key data are simple ascii (uppercased).
//

// Movement
#define KEY_RIGHTARROW	0xae
#define KEY_LEFTARROW	0xac
#define KEY_UPARROW		0xad
#define KEY_DOWNARROW	0xaf
#define KEY_STRAFE_L	0xa0
#define KEY_STRAFE_R	0xa1

// Control
#define KEY_USE			0xa2
#define KEY_FIRE		0xa3
#define KEY_ESCAPE		27

#endif

// Control
#define KEY_ENTER		13

#define KEY_TAB			9
#define KEY_F1			(0x80+0x3b)
#define KEY_F2			(0x80+0x3c)
#define KEY_F3			(0x80+0x3d)
#define KEY_F4			(0x80+0x3e)
#define KEY_F5			(0x80+0x3f)
#define KEY_F6			(0x80+0x40)
#define KEY_F7			(0x80+0x41)
#define KEY_F8			(0x80+0x42)
#define KEY_F9			(0x80+0x43)
#define KEY_F10			(0x80+0x44)
#define KEY_F11			(0x80+0x57)
#define KEY_F12			(0x80+0x58)

#define KEY_BACKSPACE	0x7f
#define KEY_PAUSE	0xff

#define KEY_EQUALS	0x3d
#define KEY_MINUS	0x2d

#define KEY_RSHIFT	(0x80+0x36)
#define KEY_RCTRL	(0x80+0x1d)
#define KEY_RALT	(0x80+0x38)

#define KEY_LALT	KEY_RALT

// new keys:

#define KEY_CAPSLOCK    (0x80+0x3a)
#define KEY_NUMLOCK     (0x80+0x45)
#define KEY_SCRLCK      (0x80+0x46)
#define KEY_PRTSCR      (0x80+0x59)

#define KEY_HOME        (0x80+0x47)
#define KEY_END         (0x80+0x4f)
#define KEY_PGUP        (0x80+0x49)
#define KEY_PGDN        (0x80+0x51)
#define KEY_INS         (0x80+0x52)
#define KEY_DEL         (0x80+0x53)

#define KEYP_0          0
#define KEYP_1          KEY_END
#define KEYP_2          KEY_DOWNARROW
#define KEYP_3          KEY_PGDN
#define KEYP_4          KEY_LEFTARROW
#define KEYP_5          '5'
#define KEYP_6          KEY_RIGHTARROW
#define KEYP_7          KEY_HOME
#define KEYP_8          KEY_UPARROW
#define KEYP_9          KEY_PGUP

#define KEYP_DIVIDE     '/'
#define KEYP_PLUS       '+'
#define KEYP_MINUS      '-'
#define KEYP_MULTIPLY   '*'
#define KEYP_PERIOD     0
#define KEYP_EQUALS     KEY_EQUALS
#define KEYP_ENTER      KEY_ENTER

#endif          // __DOOMKEYS__

