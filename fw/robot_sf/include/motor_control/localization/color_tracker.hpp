#pragma once
#include "localization/types.hpp"
#include <cstddef>

namespace loc {
struct RgbcSample {
  uint16_t red = 0, green = 0, blue = 0, clear = 0;
  uint32_t sequence = 0;
  bool saturated = false;
};
struct ColorReference {
  float r, g, b; // Calibrated RGB / (R+G+B), not factory color names.
  MarkerGeometry geometry;
  int gx, gy; // CELL: 0..4; X_LINE gx or Y_LINE gy: 0..5.
};
class ColorTracker {
public:
  /** @brief Classify a fresh frame and emit one event per stable mapped entry.
   * @param[in] raw RGBC frame with strictly increasing nonzero sequence.
   * @param[in] refs Board-calibrated, unambiguous map associations; may be
   * null.
   * @param[in] count Number of references; zero disables color localization.
   * @param[out] out Non-null destination for a debounced mapped event.
   * @returns true once after three matching fresh frames; false otherwise.
   * @note Three unknown frames rearm the same color. Ambiguous references are
   * rejected. A single sensor cannot report lateral error or a unique node ID.
   */
  bool process(const RgbcSample &raw, const ColorReference *refs,
               std::size_t count, MarkerSample *out);

private:
  uint32_t last_frame_ = 0, event_id_ = 0;
  int candidate_ = -2, latched_ = -1;
  unsigned stable_ = 0;
};
} // namespace loc
