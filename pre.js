var Module = {};
var lines = [];
var pressed_keys = {};
var key_queue = [];
let frameCount = 0;
let previousFrame = '';

// Initialize display fields
function init_display(width, height) {
  // Create main display field
  const displayField = globalThis.addField("mainDisplay", "text", 0, [50, 50, 550, 450]);
  displayField.multiline = true;
  displayField.readonly = true;
  displayField.textFont = "Courier";
  
  // Create console field (single field for console too)
  const consoleField = globalThis.addField("consoleDisplay", "text", 0, [50, 460, 550, 580]);
  consoleField.multiline = true;
  consoleField.readonly = true;
  consoleField.textFont = "Courier";
  
  return { displayField, consoleField };
}

// Console handling
function print_msg(msg) {
  lines.push(msg);
  if (lines.length > 25) {
    lines.shift();
  }

  // Update console every 10 frames
  if (frameCount % 10 === 0) {
    globalThis.getField("consoleDisplay").value = lines.join('\n');
  }
}

Module.print = function(msg) {
  const max_len = 80;
  const num_lines = Math.ceil(msg.length / max_len);
  
  for (let i = 0, o = 0; i < num_lines; ++i, o += max_len) {
    print_msg(msg.substr(o, max_len));
  }
}

Module.printErr = Module.print;

// Input handling (unchanged)
function key_pressed(key_str) {
  if ("WASD".includes(key_str)) {
    key_str = key_str.toLowerCase();
    key_pressed("_"); //placeholder for shift
  }
  const keycode = key_str.charCodeAt(0);
  const doomkey = _key_to_doomkey(keycode);
  print_msg("pressed... " + key_str + " " + keycode + " ");
  if (doomkey === -1) return;
  pressed_keys[doomkey] = 2;
}

function key_down(key_str) {
  const keycode = key_str.charCodeAt(0);
  const doomkey = _key_to_doomkey(keycode);
  print_msg("key down.. " + key_str + " " + keycode + " ");
  if (doomkey === -1) return;
  pressed_keys[doomkey] = 1;
}

function key_up(key_str) {
  const keycode = key_str.charCodeAt(0);
  const doomkey = _key_to_doomkey(keycode);
  print_msg("key up... " + key_str + " " + keycode + " ");
  if (doomkey === -1) return;
  pressed_keys[doomkey] = 0;
}

// Input box reset
function reset_input_box() {
  globalThis.getField("key_input").value = "Type here for keyboard controls.";
}
app.setInterval("reset_input_box()", 1000);

// File handling
function write_file(filename, data) {
  let stream = FS.open("/"+filename, "w+");
  FS.write(stream, data, 0, data.length, 0);
  FS.close(stream);
}

// New optimized framebuffer rendering
const BLOCK_CHARS = ['█', '▓', '▒', '░'];  // From darkest to lightest

function update_framebuffer(framebuffer_ptr, framebuffer_len, width, height) {
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
}

// Optional: Performance monitoring
let lastFrameTime = performance.now();
let frameTimeLog = [];

function logPerformance() {
  const now = performance.now();
  const frameTime = now - lastFrameTime;
  frameTimeLog.push(frameTime);
  
  if (frameTimeLog.length > 60) { // Keep last 60 frames
    frameTimeLog.shift();
    const avgTime = frameTimeLog.reduce((a, b) => a + b) / frameTimeLog.length;
    print_msg(`Avg frame time: ${avgTime.toFixed(1)}ms (${(1000/avgTime).toFixed(1)} FPS)`);
    frameTimeLog = [];
  }
  
  lastFrameTime = now;
}
