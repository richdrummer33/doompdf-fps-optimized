
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
  print_msg("pressed... " + key_str + " " + keycode + " ");
  if (doomkey === -1) 
    return;

  pressed_keys[doomkey] = 2;
}

function key_down(key_str) {
  let keycode = key_str.charCodeAt(0);
  let doomkey = _key_to_doomkey(keycode);
  print_msg("key down.. " + key_str + " " + keycode + " ");
  if (doomkey === -1) 
    return;
  pressed_keys[doomkey] = 1;
}

function key_up(key_str) {
  let keycode = key_str.charCodeAt(0);
  let doomkey = _key_to_doomkey(keycode);
  print_msg("key up... " + key_str + " " + keycode + " ");
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

const REGIONS = {
  CENTER: { name: 'center', updateEvery: 1 },
  MIDDLE: { name: 'middle', updateEvery: 2 },
  OUTER: { name: 'outer', updateEvery: 3 }
};

function create_framebuffer(width, height) {
  js_buffer = [];
  for (let y = 0; y < height; y++) {
    let row = Array(width);
    for (let x = 0; x < width; x++) {
      row[x] = "_";
    }
    js_buffer.push(row);
  }
}


let frameCount = 0;

// Precalculate region lookup table
const regionLookup = new Array(height);
for(let y = 0; y < height; y++) {
  regionLookup[y] = new Array(width);
  for(let x = 0; x < width; x++) {
    // Simplified region detection
    const xThird = Math.floor(x / (width/3));
    const yThird = Math.floor(y / (height/3));
    // Center = 0, Middle = 1, Outer = 2
    regionLookup[y][x] = (xThird === 1 && yThird === 1) ? 0 : 
                         (xThird === 1 || yThird === 1) ? 1 : 2;
  }
}

function update_framebuffer(framebuffer_ptr, framebuffer_len, width, height) {
  let framebuffer = Module.HEAPU8.subarray(framebuffer_ptr, framebuffer_ptr + framebuffer_len);
  frameCount++;

  // Process rows based on frame count
  const rowStart = frameCount % 3;
  
  for(let y = rowStart; y < height; y += 3) {
    let row = js_buffer[y];
    let old_row = row.join("");
    let changed = false;

    // Process pixels in chunks of 3
    for(let x = 0; x < width; x += 3) {
      const region = regionLookup[y][x];
      
      // Process 3x3 block if needed
      if((frameCount % (region + 1)) === 0) {
        for(let dy = 0; dy < 3 && (y + dy) < height; dy++) {
          for(let dx = 0; dx < 3 && (x + dx) < width; dx++) {
            const px = x + dx;
            const index = ((y + dy) * width + px) * 4;
            const avg = (framebuffer[index] + framebuffer[index+1] + framebuffer[index+2]) / 3;
            
            if(avg > 200) row[px] = "_";
            else if(avg > 150) row[px] = "::";
            else if(avg > 100) row[px] = "?";
            else if(avg > 50) row[px] = "//";
            else if(avg > 25) row[px] = "b";
            else row[px] = "#";
          }
        }
        changed = true;
      }
    }

    if(changed) {
      let row_str = row.join("");
      if(row_str !== old_row) {
        globalThis.getField("field_" + (height-y-1)).value = row_str;
      }
    }
  }
}