// pre.js - simplified to remove duplicate ASCII conversion
// The rendering work is done in doomgeneric_pdfjs.c now(RB)

var Module = {};
var lines = [];
var pressed_keys = {};
var key_queue = [];

// Console output handling (unchanged)
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

// Key handling (unchanged)
function key_pressed(key_str) {
    if ("WASD".includes(key_str)) {
        key_str = key_str.toLowerCase();
        key_pressed("_"); //placeholder for shift
    }
    let keycode = key_str.charCodeAt(0);
    let doomkey = key_to_doomkey(keycode);
    print_msg("pressed: " + key_str + " " + keycode + " ");
    if (doomkey === -1) 
        return;
    pressed_keys[doomkey] = 2;
}

function key_down(key_str) {
    let keycode = key_str.charCodeAt(0);
    let doomkey = key_to_doomkey(keycode);
    print_msg("key down: " + key_str + " " + keycode + " ");
    if (doomkey === -1) 
        return;
    pressed_keys[doomkey] = 1;
}

function key_up(key_str) {
    let keycode = key_str.charCodeAt(0);
    let doomkey = key_to_doomkey(keycode);
    print_msg("key up: " + key_str + " " + keycode + " ");
    if (doomkey === -1) 
        return;
    pressed_keys[doomkey] = 0;
}

// Input box handling (unchanged)
function reset_input_box() {
    globalThis.getField("key_input").value = "Type here for keyboard controls.";
}
app.setInterval("reset_input_box()", 1000);

// Add to pre.js
var js_buffer = [];

function create_framebuffer(width, height) {
    js_buffer = new Array(height).fill().map(() => new Array(width).fill("_"));
    print_msg("Framebuffer created");
}

// File system handling (unchanged)
function write_file(filename, data) {
    let stream = FS.open("/"+filename, "w+");
    FS.write(stream, data, 0, data.length, 0);
    FS.close(stream);
}
