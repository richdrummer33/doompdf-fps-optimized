
// ------------------------------------//
// *** OG INPUT HANDLING ***
// ------------------------------------//
var Module = {};
var lines = [];
var pressed_keys = {};
var js_buffer = [];

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


// Pre-compute the brightness characters to avoid string allocations
const BRIGHTNESS_CHARS = {
  DARK: '#',
  DIM: 'b',
  LOW: '//',
  MED: '?',
  HIGH: '::',
  BRIGHT: '_'
};

// Pre-allocate typed arrays for better memory performance
let framebuffer;
let currentRow;
let prevRow;


function create_framebuffer(width, height) {
  js_buffer = new Array(height);
  for (let y = 0; y < height; y++) {
    js_buffer[y] = new Uint8Array(width).fill(BRIGHTNESS_CHARS.BRIGHT.charCodeAt(0));
  }
  
  // Pre-allocate buffers for row comparisons
  currentRow = new Uint8Array(width);
  prevRow = new Uint8Array(width);
  
  // Pre-allocate the framebuffer view
  framebuffer = new Uint8Array(width * height * 4);
}

let frameCount = 0;

function update_framebuffer(framebuffer_ptr, framebuffer_len, width, height) {
  // Get a view of the framebuffer without copying
  const frameView = new Uint8Array(Module.HEAPU8.buffer, framebuffer_ptr, framebuffer_len);
  
  // Update local framebuffer from view
  framebuffer.set(frameView);
  
  // Process rows
  for (let y = 0; y < height; y++) {
    const row = js_buffer[y];
    let hasChanged = false;
    
    // Direct pointer arithmetic for frame data
    let ptr = y * width * 4;
    
    for (let x = 0; x < width; x++) {
      // Fast integer averaging (shift by 2 is divide by 4)
      const avg = (framebuffer[ptr] + framebuffer[ptr + 1] + framebuffer[ptr + 2]) >> 2;
      
      // Use pre-computed brightness values with minimal branches
      const char = 
        avg > 200 ? BRIGHTNESS_CHARS.BRIGHT :
        avg > 150 ? BRIGHTNESS_CHARS.HIGH :
        avg > 100 ? BRIGHTNESS_CHARS.MED :
        avg > 50 ? BRIGHTNESS_CHARS.LOW :
        avg > 25 ? BRIGHTNESS_CHARS.DIM :
        BRIGHTNESS_CHARS.DARK;
      
      // Only mark as changed if the character actually changed
      if (row[x] !== char) {
        row[x] = char;
        hasChanged = true;
      }
      
      ptr += 4; // Move to next pixel
    }
    
    // Only update DOM if row changed
    if (hasChanged) {
      const field = globalThis.getField("field_" + (height - y - 1));
      field.value = String.fromCharCode.apply(null, row);
    }
  }
}
