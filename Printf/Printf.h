#pragma once
#include "../struct.h"

// 描画関数
void VectorScreenPrintf(int x, int y, const Vector3 &vector, const char *label);
void MatrixScreenPrintf(int x, int y, const Matrix4x4 &m, const char *label);
