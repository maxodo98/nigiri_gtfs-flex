#include <chrono>

#include <nigiri/loader/gtfs/agency.h>
#include <nigiri/loader/gtfs/route.h>
#include <nigiri/loader/gtfs/stop_time.h>
#include <nigiri/loader/gtfs/trip.h>
#include <nigiri/loader/loader_interface.h>

#include "gtest/gtest.h"

#include "nigiri/loader/gtfs/booking_rule.h"

#include "nigiri/loader/gtfs/files.h"
#include "nigiri/timetable.h"

#include "./test_data.h"

using namespace nigiri;
using namespace nigiri::loader::gtfs;
using namespace std::string_view_literals;

TEST(gtfs, loader_test) {
  timetable tt;
  tt.date_range_ = interval{date::sys_days{date::January / 1 / 2025},
                            date::sys_days{date::December / 31 / 2025}};
  auto const files = example_files();
  auto const src = source_idx_t{0};

  auto const config = loader::loader_config{};
  auto timezones = tz_map{};
  auto agencies =
      read_agencies(tt, timezones, files.get_file(kLoaderAgencyFile).data());
  auto const stops = read_stops(
      src, tt, timezones, files.get_file(kLoaderStopsFile).data(), "", 0U);
  auto const routes = read_routes(tt, timezones, agencies,
                                  files.get_file(kLoaderRoutesFile).data(), "");
  auto const calendar =
      read_calendar(files.get_file(kLoaderCalendarFile).data());
  auto const dates =
      read_calendar_date(files.get_file(kLoaderCalendarDatesFile).data());
  auto const service =
      merge_traffic_days(tt.internal_interval_days(), calendar, dates);
  auto trip_data = read_trips(tt, routes, service, {},
                              files.get_file(kLoaderTripsFile).data(),
                              config.bikes_allowed_default_);
  auto booking_rules = read_booking_rules(
      service, tt, files.get_file(kLoaderBookingRulesFile).data());

  const auto geojsons = read_location_geojson(
      tt, files.get_file(kLoaderLocationGeojsonFile).data());

  read_stop_times(tt, src, trip_data, geojsons, stops, booking_rules,
                  files.get_file(kLoaderStopTimesFile).data(), false);

  auto const test_location = [&](std::string const& id,
                                 geo::latlng const&& expected_pos) {
    ASSERT_TRUE(stops.contains(id));
    ASSERT_LE(stops.at(id), tt.locations_.coordinates_.size());
    EXPECT_EQ(tt.locations_.coordinates_[stops.at(id)], expected_pos);
  };
  auto const test_booking_rules =
      [&](std::string const& id, booking_rule const&& expected_booking_rule) {
        ASSERT_TRUE(booking_rules.contains(id));
        ASSERT_LE(booking_rules.at(id), tt.booking_rules_.size());
        auto const& actual_booking_rule =
            tt.booking_rules_[booking_rules.at(id)];
        EXPECT_EQ(actual_booking_rule.type_, expected_booking_rule.type_);
        EXPECT_EQ(actual_booking_rule.prior_notice_duration_min_,
                  expected_booking_rule.prior_notice_duration_min_);
        EXPECT_EQ(actual_booking_rule.prior_notice_duration_max_,
                  expected_booking_rule.prior_notice_duration_max_);
        EXPECT_EQ(actual_booking_rule.prior_notice_last_day_,
                  expected_booking_rule.prior_notice_last_day_);
        EXPECT_EQ(actual_booking_rule.prior_notice_last_time_,
                  expected_booking_rule.prior_notice_last_time_);
        EXPECT_EQ(actual_booking_rule.prior_notice_start_day_,
                  expected_booking_rule.prior_notice_start_day_);
        EXPECT_EQ(actual_booking_rule.prior_notice_start_time_,
                  expected_booking_rule.prior_notice_start_time_);
      };
  auto const test_location_geojson =
      [&](std::string const& id, multipolgyon const&& expected_multipolgyon,
          std::vector<location_idx_t>&& expected_locations_within,
          std::vector<trip_idx_t>&& expected_related_trips) {
        ASSERT_TRUE(geojsons.contains(id));
        ASSERT_LE(geojsons.at(id), tt.geometry_.size());

        auto* expected_tg_geo = create_tg_multipoly(expected_multipolgyon);
        auto* actual_tg_geo =
            create_tg_multipoly(tt.geometry_[geojsons.at(id)]);

        EXPECT_TRUE(tg_geom_covers(actual_tg_geo, expected_tg_geo));
        tg_geom_free(expected_tg_geo);

        for (auto const location : expected_locations_within) {
          auto const pos = tt.locations_.coordinates_[location];
          EXPECT_TRUE(tg_geom_covers(
              actual_tg_geo, tg_geom_new_point(tg_point{pos.lng_, pos.lat_})));
        }

        tg_geom_free(actual_tg_geo);

        ASSERT_EQ(tt.geometry_idx_to_trip_idxs_[geojsons.at(id)].size(),
                  expected_related_trips.size());
        for (auto i = 0; i < expected_related_trips.size(); ++i) {
          EXPECT_EQ(tt.geometry_idx_to_trip_idxs_[geojsons.at(id)][i],
                    expected_related_trips[i]);
        }
      };
  auto const test_trip =
      [&](std::string id, std::vector<geometry_idx_t>&& expected_geometries) {
        ASSERT_TRUE(trip_data.trips_.contains(id));
        auto const idx = trip_data.data_[trip_data.trips_[id]].trip_idx_;
        ASSERT_LT(idx, tt.trip_idx_to_geometry_idxs_.size());
        ASSERT_EQ(tt.trip_idx_to_geometry_idxs_[idx].size(),
                  expected_geometries.size());
        for (auto i = 0; i < expected_geometries.size(); ++i) {
          EXPECT_EQ(tt.trip_idx_to_geometry_idxs_[idx][i],
                    expected_geometries[i]);
        }
      };
  auto const test_stop_times =
      [&](std::string trip_id, std::string geo_id,
          stop_window const& expected_window,
          booking_rule_idx_t expected_pickup_booking_rule,
          booking_rule_idx_t expected_dropoff_booking_rule,
          pickup_dropoff_type expected_pickup_type,
          pickup_dropoff_type expected_dropoff_type) {
        ASSERT_TRUE(trip_data.trips_.contains(trip_id));
        ASSERT_TRUE(geojsons.contains(geo_id));

        auto const t_idx = trip_data.data_[trip_data.trips_[trip_id]].trip_idx_;
        auto const g_idx = geojsons.at(geo_id);

        auto const gt_id = geometry_trip_idx{t_idx, g_idx};
        ASSERT_TRUE(tt.geometry_trip_idxs_.contains(gt_id));
        auto const gt_idx = tt.geometry_trip_idxs_[gt_id];
        ASSERT_LT(gt_idx, tt.window_times_.size());

        EXPECT_EQ(tt.window_times_[gt_idx], expected_window);
        EXPECT_EQ(tt.pickup_booking_rules_[gt_idx],
                  expected_pickup_booking_rule);
        EXPECT_EQ(tt.dropoff_booking_rules_[gt_idx],
                  expected_dropoff_booking_rule);
        EXPECT_EQ(tt.pickup_types_[gt_idx], expected_pickup_type);
        EXPECT_EQ(tt.dropoff_types_[gt_idx], expected_dropoff_type);
      };

  test_location("s_1", {49.87441240201039, 8.673522730953579});
  test_location("s_2", {49.872831594042964, 8.628161540647596});
  test_location("s_3", {49.8529412214732, 8.64680776859845});
  test_location("s_4", {49.891010734582096, 8.653007984390484});
  test_location("s_5", {49.87292663386987, 8.655741396888601});
  test_location("s_6", {49.87295387982826, 8.652074395902673});
  test_location("s_7", {49.872922544554115, 8.663944746090607});
  test_location("s_8", {49.86090506822356, 8.686989336713737});
  test_location("s_9", {49.87467964052996, 8.656740285924798});

  test_booking_rules("b_Arbeitswoche", booking_rule{1, 60, 1440});
  test_booking_rules("b_Wochenende", booking_rule{1, 30, 120});
  test_booking_rules("b_Nord_Ein", booking_rule{1, 60});
  test_booking_rules("b_Nord_Aus", booking_rule{1, 90});
  test_booking_rules("b_Ost", booking_rule{.type_ = 2,
                                           .prior_notice_last_day_ = 1,
                                           .prior_notice_last_time_ =
                                               hhmm_to_min("18:00:00")});
  test_booking_rules(
      "b_Süd",
      booking_rule{.type_ = 2,
                   .prior_notice_last_day_ = 2,
                   .prior_notice_last_time_ = hhmm_to_min("12:00:00"),
                   .prior_notice_start_day_ = 7,
                   .prior_notice_start_time_ = hhmm_to_min("00:00:00")});
  test_booking_rules("b_West_Vormittag", booking_rule{1, 60});
  test_booking_rules("b_West_Nachmittag", booking_rule{1, 20});

  auto constexpr s1 = location_idx_t{0};
  auto constexpr s2 = location_idx_t{1};
  auto constexpr s3 = location_idx_t{2};
  auto constexpr s4 = location_idx_t{3};
  auto constexpr s5 = location_idx_t{4};
  auto constexpr s6 = location_idx_t{5};
  auto constexpr s7 = location_idx_t{6};
  auto constexpr s8 = location_idx_t{7};
  auto constexpr s9 = location_idx_t{8};

  auto constexpr dnw_Arbeitswoche_1 = trip_idx_t{0};
  auto constexpr dnw_Arbeitswoche_2 = trip_idx_t{1};
  auto constexpr dnw_Wochenende = trip_idx_t{2};
  auto constexpr nnno_1 = trip_idx_t{3};
  auto constexpr nnno_2 = trip_idx_t{4};
  auto constexpr onos_1 = trip_idx_t{5};
  auto constexpr onos_2 = trip_idx_t{6};
  auto constexpr sns_1 = trip_idx_t{7};
  auto constexpr sns_2 = trip_idx_t{8};
  auto constexpr sns_3 = trip_idx_t{9};
  auto constexpr wnos_1 = trip_idx_t{10};
  auto constexpr wnos_2 = trip_idx_t{11};

  test_location_geojson(
      "Nord",
      ring_to_multipolygon(ring{point{8.652484802610935, 49.88950826461542},
                                point{8.632531855280726, 49.874863308155824},
                                point{8.633456826348993, 49.87012261291535},
                                point{8.667680755875645, 49.871740746835144},
                                point{8.664157056567763, 49.88255530236867},
                                point{8.652484802610935, 49.88950826461542}}),
      {s5, s6, s7, s9}, {nnno_1, nnno_2});
  test_location_geojson(
      "Ost",
      ring_to_multipolygon(ring{point{8.669980537584316, 49.879342215997035},
                                point{8.690065623638578, 49.86838499193017},
                                point{8.688450594787996, 49.858816677986475},
                                point{8.676558109624278, 49.856857353264274},
                                point{8.66717626021736, 49.861854893978304},
                                point{8.656252792363125, 49.86920828943974},
                                point{8.65457903519201, 49.874715504945975},
                                point{8.660393139049853, 49.8821520879108},
                                point{8.669980537584316, 49.879342215997035}}),
      {s1, s5, s7, s8, s9}, {nnno_1, nnno_2, onos_1, onos_2, wnos_1, wnos_2});
  test_location_geojson(
      "Süd",
      ring_to_multipolygon(ring{point{8.638377009844826, 49.85267109261012},
                                point{8.655907413900893, 49.85312547521351},
                                point{8.666786835513683, 49.86783380401884},
                                point{8.653183072305495, 49.87444825258534},
                                point{8.638176545376032, 49.872147294725785},
                                point{8.638377009844826, 49.85267109261012}}),
      {s3, s5, s6}, {onos_1, onos_2, sns_1, sns_2, sns_3, wnos_1, wnos_2});
  test_location_geojson(
      "West",
      ring_to_multipolygon(ring{point{8.615742026526112, 49.87558530573456},
                                point{8.61992641945426, 49.86178794375207},
                                point{8.657894279495252, 49.8626113537035},
                                point{8.670047037904022, 49.87572465292158},
                                point{8.65137143157277, 49.88381384194538},
                                point{8.615605883599105, 49.87575303841029},
                                point{8.615742026526112, 49.87558530573456}}),
      {s2, s5, s6, s7, s9},
      {dnw_Arbeitswoche_1, dnw_Arbeitswoche_2, dnw_Wochenende, wnos_1, wnos_2});
  test_location_geojson(
      "Darmstadt",
      ring_to_multipolygon(ring{point{8.654200005936705, 49.89605856027628},
                                point{8.615821336359803, 49.87570244657914},
                                point{8.614919725647121, 49.846600119930265},
                                point{8.677517269409314, 49.84307687627821},
                                point{8.690268620916164, 49.86653618300896},
                                point{8.68576056735159, 49.88438253735107},
                                point{8.671141593654223, 49.88948621165312},
                                point{8.654200005936705, 49.89605856027628}}),
      {s1, s2, s3, s4, s5, s6, s7, s8, s9},
      {dnw_Arbeitswoche_1, dnw_Arbeitswoche_2, dnw_Wochenende});

  auto constexpr Nord = geometry_idx_t{0};
  auto constexpr Ost = geometry_idx_t{1};
  auto constexpr Süd = geometry_idx_t{2};
  auto constexpr West = geometry_idx_t{3};
  auto constexpr Darmstadt = geometry_idx_t{4};

  test_trip("dnw_Arbeitswoche_1", {Darmstadt, West});
  test_trip("dnw_Arbeitswoche_2", {Darmstadt, West});
  test_trip("dnw_Wochenende", {Darmstadt, West});
  test_trip("nnno_1", {Nord, Ost});
  test_trip("nnno_2", {Nord, Ost});
  test_trip("onos_1", {Ost, Süd});
  test_trip("onos_2", {Ost, Süd});
  test_trip("sns_1", {Süd});
  test_trip("sns_2", {Süd});
  test_trip("sns_3", {Süd});
  test_trip("wnos_1", {West, Ost, Süd});
  test_trip("wnos_2", {West, Ost, Süd});

  auto constexpr b_Arbeitswoche = booking_rule_idx_t{0};
  auto constexpr b_Wochenende = booking_rule_idx_t{1};
  auto constexpr b_Nord_Ein = booking_rule_idx_t{2};
  auto constexpr b_Nord_Aus = booking_rule_idx_t{3};
  auto constexpr b_Ost = booking_rule_idx_t{4};
  auto constexpr b_Süd = booking_rule_idx_t{5};
  auto constexpr b_West_Vormittag = booking_rule_idx_t{6};
  auto constexpr b_West_Nachmittag = booking_rule_idx_t{7};

  test_stop_times("dnw_Arbeitswoche_1", "Darmstadt",
                  {hhmm_to_min("08:00:00"), hhmm_to_min("12:00:00")},
                  b_Arbeitswoche, b_Arbeitswoche, kPhoneAgencyType,
                  kUnavailableType);
  test_stop_times("dnw_Arbeitswoche_1", "West",
                  {hhmm_to_min("08:00:00"), hhmm_to_min("12:00:00")},
                  b_Arbeitswoche, b_Arbeitswoche, kUnavailableType,
                  kPhoneAgencyType);
  test_stop_times("dnw_Arbeitswoche_2", "Darmstadt",
                  {hhmm_to_min("13:00:00"), hhmm_to_min("20:00:00")},
                  b_Arbeitswoche, b_Arbeitswoche, kPhoneAgencyType,
                  kUnavailableType);
  test_stop_times("dnw_Arbeitswoche_2", "West",
                  {hhmm_to_min("13:00:00"), hhmm_to_min("20:00:00")},
                  b_Arbeitswoche, b_Arbeitswoche, kUnavailableType,
                  kPhoneAgencyType);
  test_stop_times("dnw_Wochenende", "Darmstadt",
                  {hhmm_to_min("10:00:00"), hhmm_to_min("18:00:00")},
                  b_Wochenende, b_Wochenende, kPhoneAgencyType,
                  kUnavailableType);
  test_stop_times("dnw_Wochenende", "West",
                  {hhmm_to_min("10:00:00"), hhmm_to_min("18:00:00")},
                  b_Wochenende, b_Wochenende, kUnavailableType,
                  kPhoneAgencyType);
  test_stop_times("nnno_1", "Nord",
                  {hhmm_to_min("12:00:00"), hhmm_to_min("16:00:00")},
                  b_Nord_Ein, b_Nord_Aus, kPhoneAgencyType, kPhoneAgencyType);
  test_stop_times("nnno_1", "Ost",
                  {hhmm_to_min("14:00:00"), hhmm_to_min("16:00:00")},
                  b_Nord_Ein, b_Nord_Aus, kUnavailableType, kPhoneAgencyType);
  test_stop_times("nnno_2", "Nord",
                  {hhmm_to_min("09:00:00"), hhmm_to_min("18:00:00")},
                  b_Nord_Ein, b_Nord_Aus, kPhoneAgencyType, kPhoneAgencyType);
  test_stop_times("nnno_2", "Ost",
                  {hhmm_to_min("09:00:00"), hhmm_to_min("18:00:00")},
                  booking_rule_idx_t::invalid(), b_Nord_Aus, kUnavailableType,
                  kPhoneAgencyType);
  test_stop_times("onos_1", "Ost",
                  {hhmm_to_min("08:00:00"), hhmm_to_min("20:00:00")}, b_Ost,
                  b_Ost, kPhoneAgencyType, kPhoneAgencyType);
  test_stop_times("onos_1", "Süd",
                  {hhmm_to_min("15:00:00"), hhmm_to_min("20:00:00")}, b_Ost,
                  b_Ost, kUnavailableType, kPhoneAgencyType);
  test_stop_times("onos_2", "Ost",
                  {hhmm_to_min("06:00:00"), hhmm_to_min("12:00:00")}, b_Ost,
                  b_Ost, kPhoneAgencyType, kPhoneAgencyType);
  test_stop_times("onos_2", "Süd",
                  {hhmm_to_min("10:00:00"), hhmm_to_min("13:00:00")}, b_Ost,
                  b_Ost, kUnavailableType, kPhoneAgencyType);
  test_stop_times("sns_1", "Süd",
                  {hhmm_to_min("06:00:00"), hhmm_to_min("14:00:00")}, b_Süd,
                  b_Süd, kPhoneAgencyType, kPhoneAgencyType);
  test_stop_times("sns_2", "Süd",
                  {hhmm_to_min("10:00:00"), hhmm_to_min("18:00:00")}, b_Ost,
                  b_Ost, kPhoneAgencyType, kPhoneAgencyType);
  test_stop_times("sns_3", "Süd",
                  {hhmm_to_min("07:00:00"), hhmm_to_min("16:00:00")}, b_Süd,
                  b_Süd, kPhoneAgencyType, kPhoneAgencyType);
  test_stop_times(
      "wnos_1", "West", {hhmm_to_min("08:00:00"), hhmm_to_min("12:00:00")},
      b_West_Vormittag, b_West_Vormittag, kPhoneAgencyType, kUnavailableType);
  test_stop_times(
      "wnos_1", "Ost", {hhmm_to_min("08:00:00"), hhmm_to_min("12:00:00")},
      b_West_Vormittag, b_West_Vormittag, kUnavailableType, kPhoneAgencyType);
  test_stop_times(
      "wnos_1", "Süd", {hhmm_to_min("08:00:00"), hhmm_to_min("12:00:00")},
      b_West_Vormittag, b_West_Vormittag, kUnavailableType, kPhoneAgencyType);
  test_stop_times(
      "wnos_2", "West", {hhmm_to_min("13:00:00"), hhmm_to_min("18:00:00")},
      b_West_Nachmittag, b_West_Nachmittag, kPhoneAgencyType, kUnavailableType);
  test_stop_times(
      "wnos_2", "Ost", {hhmm_to_min("13:00:00"), hhmm_to_min("18:00:00")},
      b_West_Nachmittag, b_West_Nachmittag, kUnavailableType, kPhoneAgencyType);
  test_stop_times(
      "wnos_2", "Süd", {hhmm_to_min("13:00:00"), hhmm_to_min("18:00:00")},
      b_West_Nachmittag, b_West_Nachmittag, kUnavailableType, kPhoneAgencyType);
}