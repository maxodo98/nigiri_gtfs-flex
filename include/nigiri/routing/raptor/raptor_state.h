#pragma once

#include <array>
#include <span>
#include <vector>

#include "date/date.h"

#include "cista/containers/bitvec.h"
#include "cista/containers/flat_matrix.h"

#include "nigiri/common/delta_t.h"
#include "nigiri/common/flat_matrix_view.h"
#include "nigiri/routing/limits.h"

namespace nigiri {
struct timetable;
}

namespace nigiri::routing {

struct raptor_state {
  raptor_state() = default;
  raptor_state(raptor_state const&) = delete;
  raptor_state& operator=(raptor_state const&) = delete;
  raptor_state(raptor_state&&) = default;
  raptor_state& operator=(raptor_state&&) = default;
  ~raptor_state() = default;

  raptor_state& resize(unsigned n_locations,
                       unsigned n_routes,
                       unsigned n_rt_transports);

  template <via_offset_t Vias>
  void print(timetable const& tt, date::sys_days, delta_t invalid);

  template <via_offset_t Vias>
  std::span<std::array<delta_t, Vias + 1>> get_tmp() {
    return {
        reinterpret_cast<std::array<delta_t, Vias + 1>*>(tmp_storage_.data()),
        n_locations_};
  }

  template <via_offset_t Vias>
  std::span<std::array<delta_t, Vias + 1>> get_best() {
    return {
        reinterpret_cast<std::array<delta_t, Vias + 1>*>(best_storage_.data()),
        n_locations_};
  }

  template <via_offset_t Vias>
  std::span<std::array<delta_t, Vias + 1> const> get_best() const {
    return {reinterpret_cast<std::array<delta_t, Vias + 1> const*>(
                best_storage_.data()),
            n_locations_};
  }

  template <via_offset_t Vias>
  flat_matrix_view<std::array<delta_t, Vias + 1>> get_round_times_end() {
    return get_round_times<Vias, false>();
  }

  template <via_offset_t Vias>
  flat_matrix_view<std::array<delta_t, Vias + 1> const> get_round_times_end()
      const {
    return get_round_times<Vias, false>();
  }

  template <via_offset_t Vias>
  flat_matrix_view<std::array<delta_t, Vias + 1>> get_round_times_start() {
    return get_round_times<Vias, true>();
  }

  template <via_offset_t Vias>
  flat_matrix_view<std::array<delta_t, Vias + 1> const> get_round_times_start()
      const {
    return get_round_times<Vias, true>();
  }

  unsigned n_locations_{};
  std::vector<delta_t> tmp_storage_;
  std::vector<delta_t> best_storage_;
  std::vector<delta_t> round_times_start_storage_;
  std::vector<delta_t> round_times_end_storage_;
  bitvec station_mark_;
  bitvec prev_station_mark_;
  bitvec route_mark_;
  bitvec rt_transport_mark_;
  bitvec end_reachable_;

private:
  template <via_offset_t Vias, bool Start_Storage>
  flat_matrix_view<std::array<delta_t, Vias + 1>> get_round_times() {
    return {{reinterpret_cast<std::array<delta_t, Vias + 1>*>(
                 Start_Storage ? round_times_start_storage_.data()
                               : round_times_end_storage_.data()),
             n_locations_ * (kMaxTransfers + 1)},
            kMaxTransfers + 1U,
            n_locations_};
  }

  template <via_offset_t Vias, bool Start_Storage>
  flat_matrix_view<std::array<delta_t, Vias + 1> const> get_round_times()
      const {
    return {{reinterpret_cast<std::array<delta_t, Vias + 1> const*>(
                 Start_Storage ? round_times_start_storage_.data()
                               : round_times_end_storage_.data()),
             n_locations_ * (kMaxTransfers + 1)},
            kMaxTransfers + 1U,
            n_locations_};
  }
};

}  // namespace nigiri::routing
