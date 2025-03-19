#include <chrono>

#include <nigiri/loader/gtfs/agency.h>
#include <nigiri/loader/gtfs/load_timetable.h>
#include <nigiri/loader/gtfs/route.h>
#include <nigiri/loader/gtfs/stop_time.h>
#include <nigiri/loader/gtfs/trip.h>
#include <nigiri/loader/loader_interface.h>

#include "gtest/gtest.h"

#include "nigiri/loader/gtfs/booking_rule.h"
#include "nigiri/loader/gtfs/files.h"
#include "nigiri/loader/gtfs/loader.h"
#include "nigiri/timetable.h"

using namespace nigiri;
using namespace nigiri::loader::gtfs;
using namespace std::string_view_literals;

TEST(gtfs, switzerland_import_test) {
  auto const gtfs_src = source_idx_t{0};
  auto const gtfs_flex_src = source_idx_t{1};
  auto const gtfs_path = loader::make_dir(
      std::filesystem::path("test/resources/switzerland_gtfs.zip"));
  auto const gtfs_flex_path = loader::make_dir(
      std::filesystem::path("test/resources/switzerland_gtfs_flex.zip"));
  auto tt = timetable{};
  auto local_bitfield_indices = nigiri::hash_map<bitfield, bitfield_idx_t>{};
  load_timetable({.default_tz_ = "Europe/Zurich"}, gtfs_src, *gtfs_path, tt,
                 local_bitfield_indices, nullptr, nullptr);

  EXPECT_EQ(tt.locations_.coordinates_.size(), 46053);
  EXPECT_EQ(tt.booking_rules_.size(), 0);
  EXPECT_EQ(tt.geometry_.size(), 0);
  EXPECT_EQ(tt.trip_service_.size(), 220197);
  EXPECT_EQ(tt.trip_idx_to_geometry_idxs_.size(), 220197);

  std::cout
      << "------------------GTFS Import Finished--------------------------\n";

  load_timetable({.default_tz_ = "Europe/Zurich"}, gtfs_flex_src,
                 *gtfs_flex_path, tt, local_bitfield_indices, nullptr, nullptr);

  std::cout << "------------------GTFS-Flex Import "
               "Finished--------------------------\n";

  EXPECT_EQ(tt.locations_.coordinates_.size(), 46053 + 703);
  EXPECT_EQ(tt.booking_rules_.size(), 12);
  EXPECT_EQ(tt.geometry_.size(), 8);
  EXPECT_EQ(tt.trip_service_.size(), 220197 + 53);
  EXPECT_EQ(tt.trip_idx_to_geometry_idxs_.size(), 220197 + 53);
}

TEST(gtfs, australia_import_test) {
  auto const gtfs_src = source_idx_t{0};
  auto const gtfs_flex_src = source_idx_t{1};
  auto const gtfs_path = loader::make_dir(
      std::filesystem::path("test/resources/australia_gtfs.zip"));
  auto const gtfs_flex_path = loader::make_dir(
      std::filesystem::path("test/resources/australia_gtfs_flex.zip"));
  auto tt = timetable{};
  auto local_bitfield_indices = nigiri::hash_map<bitfield, bitfield_idx_t>{};
  load_timetable({.default_tz_ = "Australia/Sydney"}, gtfs_src, *gtfs_path, tt,
                 local_bitfield_indices, nullptr, nullptr);

  EXPECT_EQ(tt.locations_.coordinates_.size(), 161224);
  EXPECT_EQ(tt.booking_rules_.size(), 0);
  EXPECT_EQ(tt.geometry_.size(), 0);
  EXPECT_EQ(tt.trip_service_.size(), 193385);
  EXPECT_EQ(tt.trip_idx_to_geometry_idxs_.size(), 193385);

  std::cout
      << "------------------GTFS Import Finished--------------------------\n";

  load_timetable({.default_tz_ = "Europe/Zurich"}, gtfs_flex_src,
                 *gtfs_flex_path, tt, local_bitfield_indices, nullptr, nullptr);

  std::cout << "------------------GTFS-Flex Import "
               "Finished--------------------------\n";

  EXPECT_EQ(tt.locations_.coordinates_.size(), 161224 + 18);
  EXPECT_EQ(tt.booking_rules_.size(), 12);
  EXPECT_EQ(tt.geometry_.size(), 27);
  EXPECT_EQ(tt.trip_service_.size(), 193385 + 55);
  EXPECT_EQ(tt.trip_idx_to_geometry_idxs_.size(), 193385 + 55);
}