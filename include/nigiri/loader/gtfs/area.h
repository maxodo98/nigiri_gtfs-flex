#pragma once

#include "nigiri/types.h"
#include "location_geojson.h"
#include "stop.h"

namespace nigiri {
struct timetable;
}

namespace nigiri::loader::gtfs {
using area_map_t = hash_map<std::string, area_idx>;

area_map_t read_areas(source_idx_t const stop_areas_src,
                      timetable& tt,
                      std::string_view const stop_areas_content,
                      std::string_view const location_groups_content,
                      std::string_view const location_group_stops_content);

area_map_t read_areas(source_idx_t const area_src,
                      timetable& tt,
                      std::string_view const file_content);
}  // namespace nigiri::loader::gtfs