#include <emscripten.h>

int main () {
  int a = 1;
  int b = 2;

  int c = a + b;

  EM_ASM({
    app.alert($0);
  }, c);
  return 0;
}

