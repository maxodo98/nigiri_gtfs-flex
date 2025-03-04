#pragma once

#include "nigiri/types.h"

namespace nigiri {
struct flex {
  location_idx_t target_;
  geometry_idx_t from_geometry_, target_geometry_;
  trip_idx_t trip_;
  duration_t duration_;
  transport_mode_id_t transport_mode_id_;
};
};  // namespace nigiri