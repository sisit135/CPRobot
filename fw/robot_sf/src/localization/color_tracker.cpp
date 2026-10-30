#include "localization/color_tracker.hpp"
#include <cmath>
#include <limits>

namespace loc {
bool ColorTracker::process(const RgbcSample &raw, const ColorReference *refs,
                           std::size_t count, MarkerSample *out) {
  if (!out || raw.sequence == 0 || raw.sequence <= last_frame_)
    return false;
  last_frame_ = raw.sequence;
  int best = -1;
  float first = 1e9f, second = 1e9f;
  const float sum = float(raw.red) + raw.green + raw.blue;
  if (refs && count <= 64 && !raw.saturated && raw.clear >= 100 && sum > 0) {
    for (std::size_t i = 0; i < count; ++i) {
      const float dr = raw.red / sum - refs[i].r;
      const float dg = raw.green / sum - refs[i].g;
      const float db = raw.blue / sum - refs[i].b;
      const float d = dr * dr + dg * dg + db * db;
      if (!std::isfinite(d))
        continue;
      if (d < first) {
        second = first;
        first = d;
        best = static_cast<int>(i);
      } else if (d < second)
        second = d;
    }
    if (first > 0.0144f || second - first < 0.004f)
      best = -1;
  }
  if (best != candidate_) {
    candidate_ = best;
    stable_ = 1;
  } else if (stable_ < 3)
    ++stable_;
  if (stable_ < 3)
    return false;
  if (best < 0) {
    latched_ = -1;
    return false;
  }
  if (latched_ == best || event_id_ == std::numeric_limits<uint32_t>::max())
    return false;
  latched_ = best;
  *out = {};
  out->detected = true;
  out->geometry = refs[best].geometry;
  out->gx = refs[best].gx;
  out->gy = refs[best].gy;
  out->observation_id = ++event_id_;
  out->confidence = 0.8f; // Quality weight, NOT a calibrated probability.
  return true;
}
} // namespace loc
