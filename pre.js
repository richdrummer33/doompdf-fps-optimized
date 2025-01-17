
// ------------------------------------//
// *** OG INPUT HANDLING ***
// ------------------------------------//
var Module = {};
var lines = [];
var pressed_keys = {};
var key_queue = [];

// prints a message to the console for debugging
function print_msg(msg) {
  lines.push(msg);
  if (lines.length > 25) 
    lines.shift();
  
  for (var i = 0; i < lines.length; i++) {
    var row = lines[i];
    globalThis.getField("console_"+(25-i-1)).value = row;
  }
}

Module.print = function(msg) {
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

// Maybe savedata?
function write_file(filename, data) {
  let stream = FS.open("/"+filename, "w+");
  FS.write(stream, data, 0, data.length, 0);
  FS.close(stream);
}


// ------------------------------------//
// *** RICH'S DOOM SHADER OPTIMIZATIONS  ***
// ------------------------------------//

// Brightness lookup table
const brightnessMap = [
  { limit: 25, char: "#" },
  { limit: 50, char: "b" },
  { limit: 100, char: "//" },
  { limit: 150, char: "?" },
  { limit: 200, char: "::" },
  { limit: Infinity, char: "_" }
];

// Cached field references
let fieldRefs = [];
var js_buffer = [];
var prev_buffer = [];

// Cache for common row patterns, with size limit to prevent memory bloat
const commonPatterns = new Map();
const MAX_CACHE_SIZE = 100; // Adjust based on testing

function getAsciiChar(avg) {
  for (let entry of brightnessMap) {
    if (avg <= entry.limit) return entry.char;
  }
  return "_"; // Fallback, though this should never happen due to Infinity limit
}

function create_framebuffer(width, height) {
  js_buffer = [];
  prev_buffer = [];
  
  // Initialize buffers
  for (let y = 0; y < height; y++) {
    js_buffer.push(new Array(width).fill("_"));
    prev_buffer.push(new Array(width).fill("_"));
  }
  
  // Cache field references
  fieldRefs = [];
  for (let y = 0; y < height; y++) {
    fieldRefs[y] = globalThis.getField("field_" + (height-y-1));
  }
  
  commonPatterns.clear(); // Reset cache on new frame buffer creation
}

function update_framebuffer(framebuffer_ptr, framebuffer_len, width, height) {
  let framebuffer = Module.HEAPU8.subarray(framebuffer_ptr, framebuffer_ptr + framebuffer_len);
  
  // Check if any movement keys are pressed (WASD)
  const isMoving = Object.values(pressed_keys).some(v => v > 0);
  // Skip factor: process every Nth pixel when moving
  const skipFactor = isMoving ? 2 : 1;  // Can be adjusted (2 = half resolution)
  
  for (let y = 0; y < height; y += skipFactor) {
    let row = js_buffer[y];
    let old_row = row.join("");
    
    for (let x = 0; x < width; x += skipFactor) {
      let index = (y * width + x) * 4;
      let r = framebuffer[index];
      let g = framebuffer[index+1];
      let b = framebuffer[index+2];
      let avg = (r + g + b) / 3;
      
      // Update current pixel
      if (avg > 200) row[x] = "_";
      else if (avg > 150) row[x] = "::";
      else if (avg > 100) row[x] = "?";
      else if (avg > 50) row[x] = "//";
      else if (avg > 25) row[x] = "b";
      else row[x] = "#";
      
      // Copy the same character to skipped pixels
      if (skipFactor > 1) {
        for (let dx = 1; dx < skipFactor && (x + dx) < width; dx++) {
          row[x + dx] = row[x];
        }
      }
    }
    
    // Copy the same row content to skipped rows
    if (skipFactor > 1) {
      let row_str = row.join("");
      globalThis.getField("field_"+(height-y-1)).value = row_str;
      
      for (let dy = 1; dy < skipFactor && (y + dy) < height; dy++) {
        js_buffer[y + dy] = [...row];  // Copy row content
        globalThis.getField("field_"+(height-(y+dy)-1)).value = row_str;
      }
    } else {
      let row_str = row.join("");
      if (row_str !== old_row) {
        globalThis.getField("field_"+(height-y-1)).value = row_str;
      }
    }
  }
}