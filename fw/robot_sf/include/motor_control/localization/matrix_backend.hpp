// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#ifdef LOC_USE_CMSIS_DSP
#include <dsp/matrix_functions.h>
#endif
namespace loc::matrix {
/** @brief Multiply two row-major 4x4 matrices without aliasing inputs/output.
 * @param[in] a Left operand, 16 floats.
 * @param[in] b Right operand, 16 floats.
 * @param[out] out Destination, 16 floats, distinct from both inputs.
 * @returns true on success; false if the CMSIS routine reports an error.
 */
inline bool multiply(const float *a, const float *b, float *out) {
#ifdef LOC_USE_CMSIS_DSP
  arm_matrix_instance_f32 A{4, 4, const_cast<float *>(a)},
      B{4, 4, const_cast<float *>(b)}, C{4, 4, out};
  return arm_mat_mult_f32(&A, &B, &C) == ARM_MATH_SUCCESS;
#else
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      float sum = 0;
      for (int k = 0; k < 4; ++k)
        sum += a[i * 4 + k] * b[k * 4 + j];
      out[i * 4 + j] = sum;
    }
  return true;
#endif
}
/** @brief Transpose a row-major 4x4 matrix into a distinct buffer.
 * @param[in] a Input, 16 floats.
 * @param[out] out Output, 16 floats; must not alias a.
 * @returns true on success; false on a CMSIS status error.
 */
inline bool transpose(const float *a, float *out) {
#ifdef LOC_USE_CMSIS_DSP
  arm_matrix_instance_f32 A{4, 4, const_cast<float *>(a)}, B{4, 4, out};
  return arm_mat_trans_f32(&A, &B) == ARM_MATH_SUCCESS;
#else
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      out[i * 4 + j] = a[j * 4 + i];
  return true;
#endif
}
} // namespace loc::matrix
