#include "Touch.h"

#include <M5Unified.h>

namespace t5m {

int readM5Touch(TouchPoint out[kMaxTouches]) {
  const int n = M5.Touch.getCount();
  int k = 0;
  for (int i = 0; i < n && k < kMaxTouches; i++) {
    const auto &d = M5.Touch.getDetail(i);
    if (!d.isPressed()) continue;
    out[k].id = d.id;
    out[k].x = d.x;
    out[k].y = d.y;
    out[k].down = true;
    k++;
  }
  for (int i = k; i < kMaxTouches; i++) out[i] = TouchPoint();
  return k;
}

}  // namespace t5m
