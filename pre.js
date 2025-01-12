var Module = {};
var lines = [];

function print_msg(msg) {
  lines.push(msg);
  if (lines.length > 20) 
    lines.shift();
  
  for (var i = 0; i < lines.length; i++) {
    var row = lines[i];
    globalThis.getField("console_"+(20-i-1)).value = row;
  }
}

Module.print = function(msg) {
  var max_len = 80;
  var num_lines = Math.ceil(msg.length / max_len);
  
  for (let i = 0, o = 0; i < num_lines; ++i, o += max_len) {
    print_msg(msg.substr(o, max_len));
  }
}
Module.printErr = function(msg) {
  app.alert(msg);
}
