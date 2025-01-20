var Module = {};
var lines = [];
var pressed_keys = {};
var key_queue = [];
let frameCount = 0;


// Add initialization check
/*
function print_msg(msg) {
  if (!globalThis.getField) {
    console.log(msg); // Fallback to console during early init
    return;
  }
  
  lines.push(msg);
  if (lines.length > 25) {
    lines.shift();
  }

  // Update console every 10 frames
  if (frameCount % 10 === 0) {
    const consoleField = globalThis.getField("consoleDisplay");
    if (consoleField) {
      consoleField.value = lines.join('\n');
    }
  }
}*/

function print_msg(msg) {
  lines.push(msg);
  if (lines.length > 25) 
    lines.shift();
  
  for (var i = 0; i < lines.length; i++) {
    var row = lines[i];
    globalThis.getField("console_"+(25-i-1)).value = row;
  }
}

Module.print = function (msg) {
  if (!msg || msg.length === 0)  
    return;

  let max_len = 80;
  let num_lines = Math.ceil(msg.length / max_len);
  
  for (let i = 0, o = 0; i < num_lines; ++i, o += max_len) {
    print_msg(msg.substr(o, max_len));
  }
}
Module.printErr = function(msg) {
  print_msg(msg);
}

function key_pressed(key_str) {
  if ("WASD".includes(key_str)) {
    key_str = key_str.toLowerCase();
    key_pressed("_") //placeholder for shift;
  }
  let keycode = key_str.charCodeAt(0);
  let doomkey = _key_to_doomkey(keycode);
  print_msg("pressed: " + key_str + " " + keycode + " ");
  if (doomkey === -1) 
    return;

  pressed_keys[doomkey] = 2;
}

function key_down(key_str) {
  let keycode = key_str.charCodeAt(0);
  let doomkey = _key_to_doomkey(keycode);
  print_msg("key down: " + key_str + " " + keycode + " ");
  if (doomkey === -1) 
    return;
  pressed_keys[doomkey] = 1;
}

function key_up(key_str) {
  let keycode = key_str.charCodeAt(0);
  let doomkey = _key_to_doomkey(keycode);
  print_msg("key up: " + key_str + " " + keycode + " ");
  if (doomkey === -1) 
    return;
  pressed_keys[doomkey] = 0;
}

function reset_input_box() {
  globalThis.getField("key_input").value = "Type here for keyboard controls.";
}
app.setInterval("reset_input_box()", 1000);

function write_file(filename, data) {
  let stream = FS.open("/"+filename, "w+");
  FS.write(stream, data, 0, data.length, 0);
  FS.close(stream);
}

// ======================================================================
// ============================ RENDER FUNCS ============================
// ======================================================================


function update_ascii_frame(buffer_ptr, buffer_size, width, height) {
    const asciiBuffer = Module.HEAPU8.subarray(buffer_ptr, buffer_ptr + buffer_size);
    let currentLine = '';
  let lineNum = 0;
  
  if (frameCount % 10 === 0) {
    print_msg("framebuffer size: " + buffer_size + " width: " + width + " height: " + height);
  }
    
    for (let i = 0; i < asciiBuffer.length; i++) {
        const byte = asciiBuffer[i];
        if (byte === 10) { // '\n' character
            globalThis.getField("field_" + (height - lineNum - 1)).value = currentLine;
            currentLine = '';
            lineNum++;
        } else {
            currentLine += String.fromCharCode(byte);
        }
    }
}

// New optimized framebuffer rendering (no c-optimizations)
/*let previousFrame = '';
const BLOCK_CHARS = ['█', '▓', '▒', '░'];  // From darkest to lightest

function update_framebuffer(framebuffer_ptr, framebuffer_len, width, height) 
{
  const framebuffer = Module.HEAPU8.subarray(framebuffer_ptr, framebuffer_ptr + framebuffer_len);
  const frame = [];
  frameCount++;
  
  for (let y = 0; y < height; y++) {
    const row = [];
    for (let x = 0; x < width; x++) {
      const index = (y * width + x) * 4;
      // Fast average using bit shifting
      const avg = (framebuffer[index] + framebuffer[index+1] + framebuffer[index+2]) >> 2;
      
      // Map brightness to block characters
      const charIndex = Math.min(3, Math.floor(avg / 64));
      row.push(BLOCK_CHARS[3 - charIndex]);  // Reverse index since our array goes dark to light
    }
    frame.push(row.join(''));
  }
  
  const newFrame = frame.join('\n');
  
  // Only update if frame changed
  if (newFrame !== previousFrame) {
    globalThis.getField("mainDisplay").value = newFrame;
    previousFrame = newFrame;
  }
}*/
