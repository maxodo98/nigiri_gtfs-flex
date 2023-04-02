#include "doctest/doctest.h"

#include "nigiri/loader/hrd/load_timetable.h"
#include "nigiri/routing/raptor.h"

#include "nigiri/routing/search_state.h"

#include "../loader/hrd/hrd_timetable.h"

using namespace nigiri;
using namespace nigiri::test_data::hrd_timetable;

constexpr auto const fwd_journeys = R"(
[2020-03-30 05:20, 2020-03-30 08:00]
TRANSFERS: 1
     FROM: (START, START) [2020-03-30 05:20]
       TO: (END, END) [2020-03-30 08:00]
leg 0: (START, START) [2020-03-30 05:20] -> (A, 0000001) [2020-03-30 05:30]
  MUMO (id=99, duration=10)
leg 1: (A, 0000001) [2020-03-30 05:30] -> (B, 0000002) [2020-03-30 06:30]
   0: 0000001 A...............................................                               d: 30.03 05:30 [30.03 07:30]  [{name=RE 1337, day=2020-03-30, id=1337/0000001/330/0000002/390/, src=0}]
   1: 0000002 B............................................... a: 30.03 06:30 [30.03 08:30]
leg 2: (B, 0000002) [2020-03-30 06:30] -> (B, 0000002) [2020-03-30 06:32]
  FOOTPATH (duration=2)
leg 3: (B, 0000002) [2020-03-30 06:45] -> (C, 0000003) [2020-03-30 07:45]
   0: 0000002 B...............................................                               d: 30.03 06:45 [30.03 08:45]  [{name=RE 7331, day=2020-03-30, id=7331/0000002/405/0000003/465/, src=0}]
   1: 0000003 C............................................... a: 30.03 07:45 [30.03 09:45]
leg 4: (C, 0000003) [2020-03-30 07:45] -> (END, END) [2020-03-30 08:00]
  MUMO (id=77, duration=15)


[2020-03-30 05:50, 2020-03-30 08:30]
TRANSFERS: 1
     FROM: (START, START) [2020-03-30 05:50]
       TO: (END, END) [2020-03-30 08:30]
leg 0: (START, START) [2020-03-30 05:50] -> (A, 0000001) [2020-03-30 06:00]
  MUMO (id=99, duration=10)
leg 1: (A, 0000001) [2020-03-30 06:00] -> (B, 0000002) [2020-03-30 07:00]
   0: 0000001 A...............................................                               d: 30.03 06:00 [30.03 08:00]  [{name=RE 1337, day=2020-03-30, id=1337/0000001/360/0000002/420/, src=0}]
   1: 0000002 B............................................... a: 30.03 07:00 [30.03 09:00]
leg 2: (B, 0000002) [2020-03-30 07:00] -> (B, 0000002) [2020-03-30 07:02]
  FOOTPATH (duration=2)
leg 3: (B, 0000002) [2020-03-30 07:15] -> (C, 0000003) [2020-03-30 08:15]
   0: 0000002 B...............................................                               d: 30.03 07:15 [30.03 09:15]  [{name=RE 7331, day=2020-03-30, id=7331/0000002/435/0000003/495/, src=0}]
   1: 0000003 C............................................... a: 30.03 08:15 [30.03 10:15]
leg 4: (C, 0000003) [2020-03-30 08:15] -> (END, END) [2020-03-30 08:30]
  MUMO (id=77, duration=15)


)";

TEST_CASE("raptor-intermodal-forward") {
  using namespace date;

  constexpr auto const src = source_idx_t{0U};

  timetable tt;
  tt.date_range_ = full_period();
  load_timetable(src, loader::hrd::hrd_5_20_26, files_abc(), tt);
  auto state = routing::search_state{};

  auto fwd_r = routing::raptor<direction::kForward, true>{
      tt, state,
      routing::query{
          .start_time_ =
              interval<unixtime_t>{
                  unixtime_t{sys_days{2020_y / March / 30}} + 5_hours,
                  unixtime_t{sys_days{2020_y / March / 30}} + 6_hours},
          .start_match_mode_ =
              nigiri::routing::location_match_mode::kIntermodal,
          .dest_match_mode_ = nigiri::routing::location_match_mode::kIntermodal,
          .use_start_footpaths_ = true,
          .start_ = {{tt.locations_.location_id_to_idx_.at(
                          {.id_ = "0000001", .src_ = src}),
                      10_minutes, 99U}},
          .destinations_ = {{{tt.locations_.location_id_to_idx_.at(
                                  {.id_ = "0000003", .src_ = src}),
                              15_minutes, 77U}}},
          .via_destinations_ = {},
          .allowed_classes_ = bitset<kNumClasses>::max(),
          .max_transfers_ = 6U,
          .min_connection_count_ = 0U,
          .extend_interval_earlier_ = false,
          .extend_interval_later_ = false}};
  fwd_r.route();

  std::stringstream ss;
  ss << "\n";
  for (auto const& x : state.results_.at(0)) {
    x.print(ss, tt);
    ss << "\n\n";
  }
  CHECK_EQ(std::string_view{fwd_journeys}, ss.str());
};

constexpr auto const bwd_journeys = R"(
[2020-03-30 04:00, 2020-03-30 01:20]
TRANSFERS: 1
     FROM: (END, END) [2020-03-30 01:20]
       TO: (START, START) [2020-03-30 04:00]
leg 0: (END, END) [2020-03-30 01:20] -> (A, 0000001) [2020-03-30 01:30]
  MUMO (id=77, duration=10)
leg 1: (A, 0000001) [2020-03-30 01:30] -> (B, 0000002) [2020-03-30 02:30]
   0: 0000001 A...............................................                               d: 30.03 01:30 [30.03 03:30]  [{name=RE 1337, day=2020-03-30, id=1337/0000001/90/0000002/150/, src=0}]
   1: 0000002 B............................................... a: 30.03 02:30 [30.03 04:30]
leg 2: (B, 0000002) [2020-03-30 02:43] -> (B, 0000002) [2020-03-30 02:45]
  FOOTPATH (duration=2)
leg 3: (B, 0000002) [2020-03-30 02:45] -> (C, 0000003) [2020-03-30 03:45]
   0: 0000002 B...............................................                               d: 30.03 02:45 [30.03 04:45]  [{name=RE 7331, day=2020-03-30, id=7331/0000002/165/0000003/225/, src=0}]
   1: 0000003 C............................................... a: 30.03 03:45 [30.03 05:45]
leg 4: (C, 0000003) [2020-03-30 03:45] -> (START, START) [2020-03-30 04:00]
  MUMO (id=99, duration=15)


[2020-03-30 04:30, 2020-03-30 01:50]
TRANSFERS: 1
     FROM: (END, END) [2020-03-30 01:50]
       TO: (START, START) [2020-03-30 04:30]
leg 0: (END, END) [2020-03-30 01:50] -> (A, 0000001) [2020-03-30 02:00]
  MUMO (id=77, duration=10)
leg 1: (A, 0000001) [2020-03-30 02:00] -> (B, 0000002) [2020-03-30 03:00]
   0: 0000001 A...............................................                               d: 30.03 02:00 [30.03 04:00]  [{name=RE 1337, day=2020-03-30, id=1337/0000001/120/0000002/180/, src=0}]
   1: 0000002 B............................................... a: 30.03 03:00 [30.03 05:00]
leg 2: (B, 0000002) [2020-03-30 03:13] -> (B, 0000002) [2020-03-30 03:15]
  FOOTPATH (duration=2)
leg 3: (B, 0000002) [2020-03-30 03:15] -> (C, 0000003) [2020-03-30 04:15]
   0: 0000002 B...............................................                               d: 30.03 03:15 [30.03 05:15]  [{name=RE 7331, day=2020-03-30, id=7331/0000002/195/0000003/255/, src=0}]
   1: 0000003 C............................................... a: 30.03 04:15 [30.03 06:15]
leg 4: (C, 0000003) [2020-03-30 04:15] -> (START, START) [2020-03-30 04:30]
  MUMO (id=99, duration=15)


[2020-03-30 05:00, 2020-03-30 02:20]
TRANSFERS: 1
     FROM: (END, END) [2020-03-30 02:20]
       TO: (START, START) [2020-03-30 05:00]
leg 0: (END, END) [2020-03-30 02:20] -> (A, 0000001) [2020-03-30 02:30]
  MUMO (id=77, duration=10)
leg 1: (A, 0000001) [2020-03-30 02:30] -> (B, 0000002) [2020-03-30 03:30]
   0: 0000001 A...............................................                               d: 30.03 02:30 [30.03 04:30]  [{name=RE 1337, day=2020-03-30, id=1337/0000001/150/0000002/210/, src=0}]
   1: 0000002 B............................................... a: 30.03 03:30 [30.03 05:30]
leg 2: (B, 0000002) [2020-03-30 03:43] -> (B, 0000002) [2020-03-30 03:45]
  FOOTPATH (duration=2)
leg 3: (B, 0000002) [2020-03-30 03:45] -> (C, 0000003) [2020-03-30 04:45]
   0: 0000002 B...............................................                               d: 30.03 03:45 [30.03 05:45]  [{name=RE 7331, day=2020-03-30, id=7331/0000002/225/0000003/285/, src=0}]
   1: 0000003 C............................................... a: 30.03 04:45 [30.03 06:45]
leg 4: (C, 0000003) [2020-03-30 04:45] -> (START, START) [2020-03-30 05:00]
  MUMO (id=99, duration=15)


[2020-03-30 05:30, 2020-03-30 02:50]
TRANSFERS: 1
     FROM: (END, END) [2020-03-30 02:50]
       TO: (START, START) [2020-03-30 05:30]
leg 0: (END, END) [2020-03-30 02:50] -> (A, 0000001) [2020-03-30 03:00]
  MUMO (id=77, duration=10)
leg 1: (A, 0000001) [2020-03-30 03:00] -> (B, 0000002) [2020-03-30 04:00]
   0: 0000001 A...............................................                               d: 30.03 03:00 [30.03 05:00]  [{name=RE 1337, day=2020-03-30, id=1337/0000001/180/0000002/240/, src=0}]
   1: 0000002 B............................................... a: 30.03 04:00 [30.03 06:00]
leg 2: (B, 0000002) [2020-03-30 04:13] -> (B, 0000002) [2020-03-30 04:15]
  FOOTPATH (duration=2)
leg 3: (B, 0000002) [2020-03-30 04:15] -> (C, 0000003) [2020-03-30 05:15]
   0: 0000002 B...............................................                               d: 30.03 04:15 [30.03 06:15]  [{name=RE 7331, day=2020-03-30, id=7331/0000002/255/0000003/315/, src=0}]
   1: 0000003 C............................................... a: 30.03 05:15 [30.03 07:15]
leg 4: (C, 0000003) [2020-03-30 05:15] -> (START, START) [2020-03-30 05:30]
  MUMO (id=99, duration=15)


[2020-03-30 06:00, 2020-03-30 03:20]
TRANSFERS: 1
     FROM: (END, END) [2020-03-30 03:20]
       TO: (START, START) [2020-03-30 06:00]
leg 0: (END, END) [2020-03-30 03:20] -> (A, 0000001) [2020-03-30 03:30]
  MUMO (id=77, duration=10)
leg 1: (A, 0000001) [2020-03-30 03:30] -> (B, 0000002) [2020-03-30 04:30]
   0: 0000001 A...............................................                               d: 30.03 03:30 [30.03 05:30]  [{name=RE 1337, day=2020-03-30, id=1337/0000001/210/0000002/270/, src=0}]
   1: 0000002 B............................................... a: 30.03 04:30 [30.03 06:30]
leg 2: (B, 0000002) [2020-03-30 04:43] -> (B, 0000002) [2020-03-30 04:45]
  FOOTPATH (duration=2)
leg 3: (B, 0000002) [2020-03-30 04:45] -> (C, 0000003) [2020-03-30 05:45]
   0: 0000002 B...............................................                               d: 30.03 04:45 [30.03 06:45]  [{name=RE 7331, day=2020-03-30, id=7331/0000002/285/0000003/345/, src=0}]
   1: 0000003 C............................................... a: 30.03 05:45 [30.03 07:45]
leg 4: (C, 0000003) [2020-03-30 05:45] -> (START, START) [2020-03-30 06:00]
  MUMO (id=99, duration=15)


[2020-03-30 06:30, 2020-03-30 03:50]
TRANSFERS: 1
     FROM: (END, END) [2020-03-30 03:50]
       TO: (START, START) [2020-03-30 06:30]
leg 0: (END, END) [2020-03-30 03:50] -> (A, 0000001) [2020-03-30 04:00]
  MUMO (id=77, duration=10)
leg 1: (A, 0000001) [2020-03-30 04:00] -> (B, 0000002) [2020-03-30 05:00]
   0: 0000001 A...............................................                               d: 30.03 04:00 [30.03 06:00]  [{name=RE 1337, day=2020-03-30, id=1337/0000001/240/0000002/300/, src=0}]
   1: 0000002 B............................................... a: 30.03 05:00 [30.03 07:00]
leg 2: (B, 0000002) [2020-03-30 05:13] -> (B, 0000002) [2020-03-30 05:15]
  FOOTPATH (duration=2)
leg 3: (B, 0000002) [2020-03-30 05:15] -> (C, 0000003) [2020-03-30 06:15]
   0: 0000002 B...............................................                               d: 30.03 05:15 [30.03 07:15]  [{name=RE 7331, day=2020-03-30, id=7331/0000002/315/0000003/375/, src=0}]
   1: 0000003 C............................................... a: 30.03 06:15 [30.03 08:15]
leg 4: (C, 0000003) [2020-03-30 06:15] -> (START, START) [2020-03-30 06:30]
  MUMO (id=99, duration=15)


)";

TEST_CASE("raptor-intermodal-backward") {
  using namespace date;
  timetable tt;
  tt.date_range_ = full_period();
  constexpr auto const src = source_idx_t{0U};
  load_timetable(src, loader::hrd::hrd_5_20_26, files_abc(), tt);
  auto state = routing::search_state{};

  auto bwd_r = routing::raptor<direction::kBackward, true>{
      tt, state,
      routing::query{
          .start_time_ =
              interval<unixtime_t>{
                  unixtime_t{sys_days{2020_y / March / 30}} + 5_hours,
                  unixtime_t{sys_days{2020_y / March / 30}} + 6_hours},
          .start_match_mode_ =
              nigiri::routing::location_match_mode::kIntermodal,
          .dest_match_mode_ = nigiri::routing::location_match_mode::kIntermodal,
          .use_start_footpaths_ = true,
          .start_ = {nigiri::routing::offset{
              tt.locations_.location_id_to_idx_.at(
                  {.id_ = "0000003", .src_ = src}),
              15_minutes, 99U}},
          .destinations_ = {{{tt.locations_.location_id_to_idx_.at(
                                  {.id_ = "0000001", .src_ = src}),
                              10_minutes, 77U}}},
          .via_destinations_ = {},
          .allowed_classes_ = bitset<kNumClasses>::max(),
          .max_transfers_ = 6U,
          .min_connection_count_ = 3U,
          .extend_interval_earlier_ = true,
          .extend_interval_later_ = true}};
  bwd_r.route();

  std::stringstream ss;
  ss << "\n";
  for (auto const& x : state.results_.at(0)) {
    x.print(ss, tt);
    ss << "\n\n";
  }
  std::cout << "results: " << state.results_.at(0).size() << "\n";
  CHECK_EQ(std::string_view{bwd_journeys}, ss.str());
}