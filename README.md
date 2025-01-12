# DoomPDF

This is a Doom source port that runs inside a PDF file. 

Play it here: [doom.pdf](https://doompdf.pages.dev/doom.pdf)

## Javascript in a PDF

You might expect PDF files to only be comprised of static documents, but surprisingly, the PDF file format supports Javascript with its own separate standard library. Modern browsers (Chromium, Firefox) implement this as part of their PDF engines. However, the APIs that are available in the browser are much more limited. 

The full specfication for the JS in PDFs was only ever implemented by Adobe Acrobat, and it contains some ridiculous things like the ability to do [3D rendering](https://opensource.adobe.com/dc-acrobat-sdk-docs/library/jsapiref/JS_API_AcroJS.html#annot3d), make [HTTP requests](https://opensource.adobe.com/dc-acrobat-sdk-docs/library/jsapiref/JS_API_AcroJS.html#net-http), and [detect every monitor connected to the user's system](https://opensource.adobe.com/dc-acrobat-sdk-docs/library/jsapiref/JS_API_AcroJS.html#monitor). However, on Chromium and other browsers, only a tiny amount of this API surface was implemented, due to obvious security concerns. With this, we can do whatever computation we want, just with some very limited IO.

## Porting Doom

C code can be compiled to run within a PDF using and old version of Emscripten that targets [asm.js](https://en.wikipedia.org/wiki/Asm.js) instead of WebAssembly. Then, all that's needed is a way to get key inputs, and a framebuffer for the output. Inputs are fairly straightforward, since Chromium's PDF engine supports text fields and buttons. Getting a good looking and fast enough framebuffer is a lot more of a challenge though.

Previous interactive PDF projects I've seen use individual text fields that are toggled on/off to make individual pixels. However, Doom's resolution is 320x200 which would mean thousands of text fields would have to be toggled every frame, which is infeasible. Instead, this port uses a separate text field for each row in the screen, then it sets their contents to various ASCII characters. I managed to get a 6 color monochrome output this way, which is enough for things to be legible in-game. The performance of this method is pretty poor but playable, since updating all of that text takes around 80ms per frame. 

I also implemented a scrolling text console using 25 stacked text fields. The stdout stream from Emscripten is redirected to there. This let me debug a lot easier because otherwise there is no console logging method available (the proper `console.println` is unimplemented in Chrome).

## Credits

Inspired by: [horrifying-pdf-experiments](https://github.com/osnr/horrifying-pdf-experiments) and [pdftris](https://github.com/ThomasRinsma/pdftris)

## License

This repository is licensed under the GNU GPL v2.

```
ading2210/doompdf - Doom running inside a PDF file
Copyright (C) 2025 ading2210

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
```