var Module = {};
var lines = [];
var frame_count = 0;

var pressed_keys = {};
var key_queue = [];

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