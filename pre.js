var Module = {};
var lines = [];
var js_buffer = [];
var pressed_keys = {};
var key_queue = [];
let frameCount = 0;
let previousFrame = '';

// Initialize all display fields
function init_display(width, height) {
    // Create console fields (25 lines)
    for (let i = 0; i < 25; i++) {
        const consoleField = globalThis.addField("console_" + i, "text", 0, [50, 460 + (i * 15), 550, 475 + (i * 15)]);
        consoleField.readonly = true;
        consoleField.textFont = "Courier";
    }
    
    // Create display fields for each row
    for (let i = 0; i < height; i++) {
        const displayField = globalThis.addField("field_" + i, "text", 0, [50, 50 + (i * 15), 550, 65 + (i * 15)]);
        displayField.readonly = true;
        displayField.textFont = "Courier";
    }
}

// Original console handling from script 2
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
    let doomkey = *key*to_doomkey(keycode);
    print_msg("pressed: " + key_str + " " + keycode + " ");
    if (doomkey === -1) 
        return;
    pressed_keys[doomkey] = 2;
}

function key_down(key_str) {
    let keycode = key_str.charCodeAt(0);
    let doomkey = *key*to_doomkey(keycode);
    print_msg("key down: " + key_str + " " + keycode + " ");
    if (doomkey === -1) 
        return;
    pressed_keys[doomkey] = 1;
}

function key_up(key_str) {
    let keycode = key_str.charCodeAt(0);
    let doomkey = *key*to_doomkey(keycode);
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

function create_framebuffer(width, height) {
    js_buffer = [];
    for (let y=0; y < height; y++) {
        let row = Array(width);
        for (let x=0; x < width; x++) {
            row[x] = "_";
        }
        js_buffer.push(row);
    }
}

function update_framebuffer(framebuffer_ptr, framebuffer_len, width, height) {
    let framebuffer = Module.HEAPU8.subarray(framebuffer_ptr, framebuffer_ptr + framebuffer_len);
    frameCount++;
    
    for (let y=0; y < height; y++) {
        let row = js_buffer[y];
        let old_row = row.join("");
        
        for (let x=0; x < width; x++) {
            let index = (y * width + x) * 4;
            // Fast average using bit shifting
            let avg = (framebuffer[index] + framebuffer[index+1] + framebuffer[index+2]) >> 2;
            
            // Original ASCII character mapping from script 2
            if (avg > 200)
                row[x] = "_";
            else if (avg > 150)
                row[x] = "::";
            else if (avg > 100)
                row[x] = "?";
            else if (avg > 50)
                row[x] = "//";
            else if (avg > 25)
                row[x] = "b";
            else
                row[x] = "#";
        }
        
        let row_str = row.join("");
        // Only update if row changed
        if (row_str !== old_row) {
            globalThis.getField("field_"+(height-y-1)).value = row_str;
        }
    }
}
