#include <emscripten.h>

double start_time;

int get_time() {
  return EM_ASM_DOUBLE({
    return Date.now();
  });
}

int main () {
  start_time = get_time();
  EM_ASM({
    var start = Date.now();
    for (var i=0; i<200; i++) {
      if (i % 2 == 0) 
        globalThis.getField("field_"+i).value = "#".repeat(320);
      else 
        globalThis.getField("field_"+i).value = "_".repeat(320);
    }
    var end = Date.now();
    app.alert(end - start);
    //app.alert($0);
  }, start_time);
  return 0;
}

