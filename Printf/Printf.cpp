#include "Printf.h"
#include <Novice.h>

void VectorScreenPrintf(int x, int y, const Vector3 &vector,
                        const char *label) {
  Novice::ScreenPrintf(x, y, "%.2f", vector.x);
  Novice::ScreenPrintf(x + kColumnWidth, y, "%.2f", vector.y);
  Novice::ScreenPrintf(x + kColumnWidth * 2, y, "%.2f", vector.z);
  Novice::ScreenPrintf(x + kColumnWidth * 3, y, ": %s", label);
}

void MatrixScreenPrintf(int x, int y, const Matrix4x4 &m, const char *label) {
  Novice::ScreenPrintf(x, y, "%s", label);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      Novice::ScreenPrintf(x + j * kColumnWidth, y + (i + 1) * kRowHeight,
                           "%6.02f", m.m[i][j]);
    }
  }
}
