
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

let frameCount = 0;

// Define region dimensions
const REGIONS = {
  CENTER: { name: 'center', updateEvery: 1 },  // Update every frame
  MIDDLE: { name: 'middle', updateEvery: 2 },  // Update every 2nd frame
  OUTER: { name: 'outer', updateEvery: 3 }     // Update every 3rd frame
};

function create_framebuffer(width, height) {
  js_buffer = [];
  prev_buffer = [];
  
  // Create buffers
  for (let y = 0; y < height; y++) {
    js_buffer.push(new Array(width).fill("_"));
    prev_buffer.push(new Array(width).fill("_"));
  }
  
  // Create and cache field references with region prefixes
  fieldRefs = {};
  const segmentWidth = 40;
  
  for (let y = 0; y < height; y++) {
    for (let segStart = 0; segStart < width; segStart += segmentWidth) {
      const segmentKey = `${Math.floor(segStart/segmentWidth)}_${y}`;
      const region = getRegionForPosition(segStart + segmentWidth/2, y, width, height);
      const fieldName = `field_${region.name}_${segmentKey}`;
      fieldRefs[fieldName] = globalThis.getField(fieldName);
    }
  }
}

function getRegionForPosition(x, y, width, height) {
  // Define boundaries for center region
  const centerStartX = Math.floor(width * 0.3);
  const centerEndX = Math.floor(width * 0.7);
  const centerStartY = Math.floor(height * 0.3);
  const centerEndY = Math.floor(height * 0.7);
  
  // Check if position is in center region
  if (x >= centerStartX && x < centerEndX && 
      y >= centerStartY && y < centerEndY) {
    return REGIONS.CENTER;
  }
  
  // Check if position is in middle region (surrounding center)
  const middleStartX = Math.floor(width * 0.15);
  const middleEndX = Math.floor(width * 0.85);
  const middleStartY = Math.floor(height * 0.15);
  const middleEndY = Math.floor(height * 0.85);
  
  if (x >= middleStartX && x < middleEndX && 
      y >= middleStartY && y < middleEndY) {
    return REGIONS.MIDDLE;
  }
  
  return REGIONS.OUTER;
}


function update_framebuffer(framebuffer_ptr, framebuffer_len, width, height) {
  let framebuffer = Module.HEAPU8.subarray(framebuffer_ptr, framebuffer_ptr + framebuffer_len);
  frameCount++;
  
  const isMoving = Object.values(pressed_keys).some(v => v > 0);
  const segmentWidth = 40;
  
  for (let y = 0; y < height; y++) {
    let row = js_buffer[y];
    
    // Process pixels
    for (let x = 0; x < width; x++) {
      const region = getRegionForPosition(x, y, width, height);
      if (isMoving && (frameCount % region.updateEvery) !== 0) continue;
      
      let index = (y * width + x) * 4;
      let avg = (framebuffer[index] + framebuffer[index+1] + framebuffer[index+2]) / 3;
      row[x] = getAsciiChar(avg);
    }
    
    // Update text fields by segment
    for (let segStart = 0; segStart < width; segStart += segmentWidth) {
      const segEnd = Math.min(segStart + segmentWidth, width);
      const segmentContent = row.slice(segStart, segEnd).join('');
      const segmentKey = `${Math.floor(segStart/segmentWidth)}_${y}`;
      const region = getRegionForPosition(segStart + segmentWidth/2, y, width, height);
      const fieldName = `field_${region.name}_${segmentKey}`;
      
      if (!isMoving || (frameCount % region.updateEvery === 0)) {
        if (fieldRefs[fieldName]) {
          fieldRefs[fieldName].value = segmentContent;
        }
      }
    }
  }
}