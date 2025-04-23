#include "Affine.h"
#include <math.h>
#include <cmath>
#include <assert.h>

//==================================
// Vector3 関連関数
//==================================

Vector3 Add(const Vector3 &v1, const Vector3 &v2) {
  Vector3 result;

  result.x = v1.x + v2.x;
  result.y = v1.y + v2.y;
  result.z = v1.z + v2.z;

  return result;
}

Vector3 Subtract(const Vector3 &v1, const Vector3 &v2) {
  Vector3 result;

  result.x = v1.x - v2.x;
  result.y = v1.y - v2.y;
  result.z = v1.z - v2.z;

  return result;
}

Vector3 Multiply(const Vector3 &v1, float scalar) {
  Vector3 result;

  result.x = v1.x * scalar;
  result.y = v1.y * scalar;
  result.z = v1.z * scalar;

  return result;
}

float Dot(const Vector3 &v1, const Vector3 &v2) {
  Vector3 result;

  result.x = v1.x * v2.x;
  result.y = v1.y * v2.y;
  result.z = v1.z * v2.z;

  return result.x + result.y + result.z;
}

float Length(const Vector3 &v) {
  float result;

  result = v.x * v.x + v.y * v.y + v.z * v.z;

  result = sqrtf(result);

  return result;
}

Vector3 Normalize(const Vector3 &v) {
  Vector3 result;
  float length = Length(v);
  if (length != 0) {
    result.x = v.x / length;
    result.y = v.y / length;
    result.z = v.z / length;
  } else {
    result.x = 0;
    result.y = 0;
    result.z = 0;
  }
  return result;
}

Vector3 Vector3Transform(const Vector3 &vector, const Matrix4x4 &matrix) {
  Vector3 result;

  result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] +
             vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];

  result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] +
             vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];

  result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] +
             vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];

  float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] +
            vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];

  assert(w != 0.0f);

  result.x /= w;
  result.y /= w;
  result.z /= w;

  return result;
}

//==================================
// Matrix4x4 関連関数
//==================================

Matrix4x4 Add(const Matrix4x4 &m1, const Matrix4x4 &m2) {
  Matrix4x4 result;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.m[i][j] = m1.m[i][j] + m2.m[i][j];
    }
  }
  return result;
}

Matrix4x4 Subtract(const Matrix4x4 &m1, const Matrix4x4 &m2) {
  Matrix4x4 result;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.m[i][j] = m1.m[i][j] - m2.m[i][j];
    }
  }
  return result;
}

Matrix4x4 Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2) {
  Matrix4x4 result;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.m[i][j] = 0;
      for (int k = 0; k < 4; ++k) {
        result.m[i][j] += m1.m[i][k] * m2.m[k][j];
      }
    }
  }
  return result;
}

Matrix4x4 Inverse(const Matrix4x4 &m) {
  //|A|用の変数
  float det = 0;

  Matrix4x4 result;

  // 行列式の計算
  det = (m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3]) +
        (m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1]) +
        (m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2])

        - (m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1]) -
        (m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3]) -
        (m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2])

        - (m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3]) -
        (m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1]) -
        (m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2])

        + (m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1]) +
        (m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3]) +
        (m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2])

        + (m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3]) +
        (m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1]) +
        (m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2])

        - (m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1]) -
        (m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3]) -
        (m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2])

        - (m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0]) -
        (m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0]) -
        (m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0])

        + (m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0]) +
        (m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0]) +
        (m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0]);

  result.m[0][0] = 1 / det *
                   ((m.m[1][1] * m.m[2][2] * m.m[3][3]) +
                    (m.m[1][2] * m.m[2][3] * m.m[3][1]) +
                    (m.m[1][3] * m.m[2][1] * m.m[3][2]) -
                    (m.m[1][3] * m.m[2][2] * m.m[3][1]) -
                    (m.m[1][2] * m.m[2][1] * m.m[3][3]) -
                    (m.m[1][1] * m.m[2][3] * m.m[3][2]));

  result.m[0][1] = 1 / det *
                   ((-1 * m.m[0][1] * m.m[2][2] * m.m[3][3]) -
                    (m.m[0][2] * m.m[2][3] * m.m[3][1]) -
                    (m.m[0][3] * m.m[2][1] * m.m[3][2]) +
                    (m.m[0][3] * m.m[2][2] * m.m[3][1]) +
                    (m.m[0][2] * m.m[2][1] * m.m[3][3]) +
                    (m.m[0][1] * m.m[2][3] * m.m[3][2]));

  result.m[0][2] = 1 / det *
                   ((m.m[0][1] * m.m[1][2] * m.m[3][3]) +
                    (m.m[0][2] * m.m[1][3] * m.m[3][1]) +
                    (m.m[0][3] * m.m[1][1] * m.m[3][2]) -
                    (m.m[0][3] * m.m[1][2] * m.m[3][1]) -
                    (m.m[0][2] * m.m[1][1] * m.m[3][3]) -
                    (m.m[0][1] * m.m[1][3] * m.m[3][2]));

  result.m[0][3] = 1 / det *
                   ((-1 * m.m[0][1] * m.m[1][2] * m.m[2][3]) -
                    (m.m[0][2] * m.m[1][3] * m.m[2][1]) -
                    (m.m[0][3] * m.m[1][1] * m.m[2][2]) +
                    (m.m[0][3] * m.m[1][2] * m.m[2][1]) +
                    (m.m[0][2] * m.m[1][1] * m.m[2][3]) +
                    (m.m[0][1] * m.m[1][3] * m.m[2][2]));

  result.m[1][0] = 1 / det *
                   ((-1 * m.m[1][0] * m.m[2][2] * m.m[3][3]) -
                    (m.m[1][2] * m.m[2][3] * m.m[3][0]) -
                    (m.m[1][3] * m.m[2][0] * m.m[3][2]) +
                    (m.m[1][3] * m.m[2][2] * m.m[3][0]) +
                    (m.m[1][2] * m.m[2][0] * m.m[3][3]) +
                    (m.m[1][0] * m.m[2][3] * m.m[3][2]));

  result.m[1][1] = 1 / det *
                   ((m.m[0][0] * m.m[2][2] * m.m[3][3]) +
                    (m.m[0][2] * m.m[2][3] * m.m[3][0]) +
                    (m.m[0][3] * m.m[2][0] * m.m[3][2]) -
                    (m.m[0][3] * m.m[2][2] * m.m[3][0]) -
                    (m.m[0][2] * m.m[2][0] * m.m[3][3]) -
                    (m.m[0][0] * m.m[2][3] * m.m[3][2]));

  result.m[1][2] = 1 / det *
                   ((-1 * m.m[0][0] * m.m[1][2] * m.m[3][3]) -
                    (m.m[0][2] * m.m[1][3] * m.m[3][0]) -
                    (m.m[0][3] * m.m[1][0] * m.m[3][2]) +
                    (m.m[0][3] * m.m[1][2] * m.m[3][0]) +
                    (m.m[0][2] * m.m[1][0] * m.m[3][3]) +
                    (m.m[0][0] * m.m[1][3] * m.m[3][2]));

  result.m[1][3] = 1 / det *
                   ((m.m[0][0] * m.m[1][2] * m.m[2][3]) +
                    (m.m[0][2] * m.m[1][3] * m.m[2][0]) +
                    (m.m[0][3] * m.m[1][0] * m.m[2][2]) -
                    (m.m[0][3] * m.m[1][2] * m.m[2][0]) -
                    (m.m[0][2] * m.m[1][0] * m.m[2][3]) -
                    (m.m[0][0] * m.m[1][3] * m.m[2][2]));

  result.m[2][0] = 1 / det *
                   ((m.m[1][0] * m.m[2][1] * m.m[3][3]) +
                    (m.m[1][1] * m.m[2][3] * m.m[3][0]) +
                    (m.m[1][3] * m.m[2][0] * m.m[3][1]) -
                    (m.m[1][3] * m.m[2][1] * m.m[3][0]) -
                    (m.m[1][1] * m.m[2][0] * m.m[3][3]) -
                    (m.m[1][0] * m.m[2][3] * m.m[3][1]));

  result.m[2][1] = 1 / det *
                   ((-1 * m.m[0][0] * m.m[2][1] * m.m[3][3]) -
                    (m.m[0][1] * m.m[2][3] * m.m[3][0]) -
                    (m.m[0][3] * m.m[2][0] * m.m[3][1]) +
                    (m.m[0][3] * m.m[2][1] * m.m[3][0]) +
                    (m.m[0][1] * m.m[2][0] * m.m[3][3]) +
                    (m.m[0][0] * m.m[2][3] * m.m[3][1]));

  result.m[2][2] = 1 / det *
                   ((m.m[0][0] * m.m[1][1] * m.m[3][3]) +
                    (m.m[0][1] * m.m[1][3] * m.m[3][0]) +
                    (m.m[0][3] * m.m[1][0] * m.m[3][1]) -
                    (m.m[0][3] * m.m[1][1] * m.m[3][0]) -
                    (m.m[0][1] * m.m[1][0] * m.m[3][3]) -
                    (m.m[0][0] * m.m[1][3] * m.m[3][1]));

  result.m[2][3] = 1 / det *
                   ((-1 * m.m[0][0] * m.m[1][1] * m.m[2][3]) -
                    (m.m[0][1] * m.m[1][3] * m.m[2][0]) -
                    (m.m[0][3] * m.m[1][0] * m.m[2][1]) +
                    (m.m[0][3] * m.m[1][1] * m.m[2][0]) +
                    (m.m[0][1] * m.m[1][0] * m.m[2][3]) +
                    (m.m[0][0] * m.m[1][3] * m.m[2][1]));

  result.m[3][0] = 1 / det *
                   ((-1 * m.m[1][0] * m.m[2][1] * m.m[3][2]) -
                    (m.m[1][1] * m.m[2][2] * m.m[3][0]) -
                    (m.m[1][2] * m.m[2][0] * m.m[3][1]) +
                    (m.m[1][2] * m.m[2][1] * m.m[3][0]) +
                    (m.m[1][1] * m.m[2][0] * m.m[3][2]) +
                    (m.m[1][0] * m.m[2][2] * m.m[3][1]));

  result.m[3][1] = 1 / det *
                   ((m.m[0][0] * m.m[2][1] * m.m[3][2]) +
                    (m.m[0][1] * m.m[2][2] * m.m[3][0]) +
                    (m.m[0][2] * m.m[2][0] * m.m[3][1]) -
                    (m.m[0][2] * m.m[2][1] * m.m[3][0]) -
                    (m.m[0][1] * m.m[2][0] * m.m[3][2]) -
                    (m.m[0][0] * m.m[2][2] * m.m[3][1]));

  result.m[3][2] = 1 / det *
                   ((-1 * m.m[0][0] * m.m[1][1] * m.m[3][2]) -
                    (m.m[0][1] * m.m[1][2] * m.m[3][0]) -
                    (m.m[0][2] * m.m[1][0] * m.m[3][1]) +
                    (m.m[0][2] * m.m[1][1] * m.m[3][0]) +
                    (m.m[0][1] * m.m[1][0] * m.m[3][2]) +
                    (m.m[0][0] * m.m[1][2] * m.m[3][1]));

  result.m[3][3] = 1 / det *
                   ((m.m[0][0] * m.m[1][1] * m.m[2][2]) +
                    (m.m[0][1] * m.m[1][2] * m.m[2][0]) +
                    (m.m[0][2] * m.m[1][0] * m.m[2][1]) -
                    (m.m[0][2] * m.m[1][1] * m.m[2][0]) -
                    (m.m[0][1] * m.m[1][0] * m.m[2][2]) -
                    (m.m[0][0] * m.m[1][2] * m.m[2][1]));

  return result;
}

Matrix4x4 Transpose(const Matrix4x4 &m) {
  Matrix4x4 result;

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.m[i][j] = m.m[j][i];
    }
  }

  return result;
}

Matrix4x4 MakeIdentity4x4() {
  Matrix4x4 result;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      if (i == j) {
        result.m[i][j] = 1.0f;
      } else {
        result.m[i][j] = 0.0f;
      }
    }
  }
  return result;
}


//==================================
// 回転行列
//==================================

Matrix4x4 MakeRotateMatrix(ShaftType shaft, float radian) {
  Matrix4x4 result;

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.m[i][j] = 0.0f;
    }
  }

  switch (shaft) {
  case X:

    result.m[0][0] = 1.0f;
    result.m[1][1] = std::cosf(radian);
    result.m[1][2] = std::sinf(radian);
    result.m[2][1] = -std::sinf(radian);
    result.m[2][2] = std::cosf(radian);
    result.m[3][3] = 1.0f;

    break;
  case Y:

    result.m[0][0] = std::cosf(radian);
    result.m[0][2] = -std::sinf(radian);
    result.m[1][1] = 1.0f;
    result.m[2][0] = std::sinf(radian);
    result.m[2][2] = std::cosf(radian);
    result.m[3][3] = 1.0f;

    break;
  case Z:

    result.m[0][0] = std::cosf(radian);
    result.m[0][1] = std::sinf(radian);
    result.m[1][0] = -std::sinf(radian);
    result.m[1][1] = std::cosf(radian);
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;

    break;
  }

  return result;
}

//==================================
// 平行移動行列
//==================================

Matrix4x4 MakeTranslateMatrix(const Vector3 &translate) {

  Matrix4x4 result;

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      if (i == j) {
        result.m[i][j] = 1.0f;
      } else {
        result.m[i][j] = 0.0f;
      }
    }
  }

  result.m[3][0] = translate.x;
  result.m[3][1] = translate.y;
  result.m[3][2] = translate.z;

  return result;
}

//==================================
// 拡大縮小行列
//==================================

Matrix4x4 MakeScaleMatrix(const Vector3 &scale) {
  Matrix4x4 result;

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      if (i == j) {
        result.m[i][j] = (i == 0)   ? scale.x
                         : (i == 1) ? scale.y
                         : (i == 2) ? scale.z
                                    : 1.0f;
      } else {
        result.m[i][j] = 0.0f;
      }
    }
  }

  return result;
}

//==================================
// Affine関数
//==================================
Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate,
                           const Vector3 &translate) {

  // スケーリング行列 S
  Matrix4x4 resultScaleMatrix = MakeScaleMatrix(scale);

  // 回転行列 R
  Matrix4x4 resultRotateXMatrix = MakeRotateMatrix(X, rotate.x);
  Matrix4x4 resultRotateYMatrix = MakeRotateMatrix(Y, rotate.y);
  Matrix4x4 resultRotateZMatrix = MakeRotateMatrix(Z, rotate.z);
  Matrix4x4 resultRotateMatrix = Multiply(
      resultRotateXMatrix, Multiply(resultRotateYMatrix, resultRotateZMatrix));

  // 平行移動行列 T
  Matrix4x4 resultTranslateMatrix = MakeTranslateMatrix(translate);

  // アフィン変換行列 W = S * R * T
  Matrix4x4 result = Multiply(
      resultScaleMatrix, Multiply(resultRotateMatrix, resultTranslateMatrix));

  return result;
}
