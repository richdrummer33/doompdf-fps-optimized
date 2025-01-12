var Module = {};
var lines = [];
var frame_count = 0;

var key_map = {};
var key_queue = [];

function print_msg(msg) {
  lines.push(msg);
  if (lines.length > 28) 
    lines.shift();
  
  for (var i = 0; i < lines.length; i++) {
    var row = lines[i];
    globalThis.getField("console_"+(28-i-1)).value = row;
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

function key_pressed(event) {
  let keycode = event.change.charCodeAt(0);
  let doomkey = _key_to_doomkey(keycode);
  print_msg("pressed: " + event.change + " " + keycode + " ");
  if (doomkey === -1) 
    return

  key_map[doomkey] = frame_count;
}