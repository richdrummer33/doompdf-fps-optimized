
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

function update_framebuffer(framebuffer_ptr, framebuffer_len, width, height) 
{
  let framebuffer = Module.HEAPU8.subarray(framebuffer_ptr, framebuffer_ptr + framebuffer_len);
  
  // Track cache hits for pattern frequency analysis
  let cacheHits = 0;
  
  for (let y = 0; y < height; y++) {
    let row = js_buffer[y];
    let prev_row = prev_buffer[y];
    let hasChanges = false;
    
    // Process pixels in groups of 4
    for (let x = 0; x < width; x += 4) {
      for (let i = 0; i < 4 && (x + i) < width; i++) {
        let index = (y * width + x + i) * 4;
        let avg = (framebuffer[index] + framebuffer[index+1] + framebuffer[index+2]) / 3;
        
        // Use lookup table instead of if-else chain
        let newChar = getAsciiChar(avg);
        
        if (prev_row[x + i] !== newChar) {
          row[x + i] = newChar;
          prev_row[x + i] = newChar;
          hasChanges = true;
        }
      }
    }
    
    if (hasChanges) {
      const rowStr = row.join('');
      
      // Check if this pattern is already cached
      let cachedStr = commonPatterns.get(rowStr);
      
      if (!cachedStr) {
        // New pattern found
        if (commonPatterns.size >= MAX_CACHE_SIZE) {
          // Cache is full - remove least recently used pattern
          const firstKey = commonPatterns.keys().next().value;
          commonPatterns.delete(firstKey);
        }
        // Add new pattern to cache
        commonPatterns.set(rowStr, rowStr);
        cachedStr = rowStr;
      } else {
        cacheHits++;
      }
      
      // Use cached field reference instead of looking up each time
      fieldRefs[y].value = cachedStr;
    }
  }
}