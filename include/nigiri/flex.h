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

struct flex_identification {
  location_idx_t start_;
  location_idx_t dest_;
  geometry_idx_t geometry_from_;
  geometry_idx_t geometry_to_;
  trip_idx_t trip_;
};

extern std::vector<flex_identification> flex_identifications;
};  // namespace nigiri