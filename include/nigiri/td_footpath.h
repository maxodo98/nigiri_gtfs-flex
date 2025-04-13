#pragma once

#include <span>

#include "cista/reflection/comparable.h"

#include "utl/cflow.h"
#include "utl/equal_ranges_linear.h"
#include "utl/pairwise.h"

#include "nigiri/footpath.h"
#include "nigiri/types.h"
#include "routing/query.h"

namespace nigiri {

constexpr auto const kNull = unixtime_t{0_minutes};
constexpr auto const kInfeasible =
    duration_t{std::numeric_limits<duration_t::rep>::max()};

struct td_footpath {
  CISTA_FRIEND_COMPARABLE(td_footpath)
  location_idx_t target_;
  unixtime_t valid_from_;
  duration_t duration_;
  td_offset_entry_type type_;
};

template <typename T>
struct td_result {
  duration_t duration_with_waiting_time_;
  T offset_;
};

struct duration_with_waiting {
  duration_t duration_;
  duration_t waiting_time_;
};

template <direction SearchDir, typename Collection, typename T, typename Func>
std::optional<std::variant<duration_t, duration_with_waiting, td_result<T>>> get_td_result(Collection const& c, unixtime_t const t, Func const& f) {
  auto const r = to_range<SearchDir>(c);
  auto const from = r.begin();
  auto const to = r.end();

  if constexpr (SearchDir == direction::kForward) {
    T const* best = nullptr;
    T const* curr = nullptr;
    auto best_duration = footpath::kMaxDuration;
    auto arr = unixtime_t::max();

    for (auto it = from; it != to; ++it) {
      if ((curr == nullptr || curr->duration_ == footpath::kMaxDuration ||
           it->valid_from_ < t + curr->duration_) &&
          it->type_ != kLastDeparture) {
        curr = &*it;
      } else {
        auto new_duration = curr->duration_;
        if (it->type_ == kLastDeparture) {
          if (it->valid_from_ < t) {
            it += 2;  // skip first arrival and last arrival of offset
            curr = nullptr;
            continue;
          }
          auto const first_arrival = (it + 1)->valid_from_;
          new_duration = first_arrival - std::max(t, curr->valid_from_);

          it += 2;  // skip first arrival and last arrival of offset
        }
        auto const new_arr = std::max(t, curr->valid_from_) + new_duration;
        if (best == nullptr || new_arr < arr) {
          best = &*curr;
          best_duration = new_duration;
          arr = new_arr;
        }
        curr = nullptr;
      }
    }

    if (curr != nullptr && curr->duration_ != footpath::kMaxDuration) {
      auto const new_arr = std::max(t, curr->valid_from_) + curr->duration_;
      if (best == nullptr || new_arr < arr) {
        best = &*curr;
        best_duration = best->duration_;
        arr = new_arr;
      }
    }

    if (best != nullptr && best_duration != footpath::kMaxDuration) {
      return f(*best, best_duration, arr);
    }

    return std::nullopt;
  } else /* (SearchDir == direction::kBackward) */ {
    T const* best = nullptr;
    auto best_duration = footpath::kMaxDuration;
    auto dep = unixtime_t::min();

    if (from->duration_ != footpath::kMaxDuration &&
        from->valid_from_ <= t - from->duration_) {
      best = &*from;
      best_duration = from->duration_;
      dep = t - from->duration_;
    }

    using namespace std::chrono_literals;
    auto tmp_arr = unixtime_t{};
    for (auto const [a, b] : utl::pairwise(it_range{from, to})) {
      if (a.type_ == kLastDeparture) {
        tmp_arr = unixtime_t::max();
        continue;
      }
      if (b.type_ == kFirstArrival && b.valid_from_ <= t) {
        tmp_arr = std::min(a.valid_from_, t);
        continue;
      }
      if (b.type_ == kLastDeparture && a.type_ == kFirstArrival) {
        if (tmp_arr == unixtime_t::max()) {
          continue;
        }
        auto const new_dep = b.valid_from_;
        if (dep < new_dep) {
          best_duration = tmp_arr - new_dep;
          dep = new_dep;
          best = &b;
        }
        tmp_arr = unixtime_t::max();
        continue;
      }
      if (b.duration_ != footpath::kMaxDuration &&
          std::max(b.valid_from_, dep) + b.duration_ <= t &&
          interval{b.valid_from_, a.valid_from_ + 1min}.overlaps(
              interval{dep + 1min, t + 1min})) {
        tmp_arr = unixtime_t::max();
        const auto new_dep = std::min(a.valid_from_, t) - b.duration_;
        if (dep < new_dep) {
          best_duration = b.duration_;
          dep = new_dep;
          best = &b;
        }
      }
    }
    if (best != nullptr && best_duration != footpath::kMaxDuration) {
      return f(*best, best_duration, dep);
    }

    return std::nullopt;
  }
}


template <direction SearchDir, typename Collection, typename T>
std::optional<td_result<T>> get_td_result(Collection const& c,
                                          unixtime_t const t) {
  auto result = get_td_result<SearchDir, Collection, T>(c, t, [&](T const& best, duration_t const best_duration, unixtime_t const arr_or_dep) {
    if (SearchDir == direction::kForward) {
      return std::variant<duration_t, duration_with_waiting, td_result<T>>(
           td_result<T>({arr_or_dep - t, best}));
    }
    return std::variant<duration_t, duration_with_waiting, td_result<T>>(
           td_result<T>({t - arr_or_dep, best}));
  });

  if (result.has_value() && std::holds_alternative<td_result<T>>(result.value())) {
    return std::get<td_result<T>>(result.value());
  }
  return std::nullopt;
}

template <direction SearchDir, typename Collection>
std::optional<duration_with_waiting> get_td_duration_split(Collection const& c,
                                                           unixtime_t const t) {
  auto const r = to_range<SearchDir>(c);
  auto const from = r.begin();
  using Type = std::decay_t<decltype(*from)>;
  auto result =  get_td_result<SearchDir, Collection, Type>(c, t, [&](Type const& best, duration_t const best_duration, unixtime_t const arr_or_dep) {
    auto const waiting_time = (SearchDir == direction::kForward ? (arr_or_dep - t): (t - arr_or_dep)) - best_duration;
    return std::variant<duration_t, duration_with_waiting, td_result<Type>>(
                      duration_with_waiting{best_duration, waiting_time});
});
  if (result.has_value() && std::holds_alternative<duration_with_waiting>(result.value())) {
    return std::get<duration_with_waiting>(result.value());
  }
  return std::nullopt;
}

template <direction SearchDir, typename Collection>
std::optional<duration_t> get_td_duration(Collection const& c,
                                          unixtime_t const t) {
  auto const r = to_range<SearchDir>(c);
  auto const from = r.begin();
  using Type = std::decay_t<decltype(*from)>;
  auto result =  get_td_result<SearchDir, Collection, Type>(c, t, [&](Type const& best, duration_t const best_duration, unixtime_t const arr_or_dep) {
    auto const duration_with_waiting_time = SearchDir == direction::kForward ? (arr_or_dep - t): (t - arr_or_dep);
    return std::variant<duration_t, duration_with_waiting, td_result<Type>>(duration_with_waiting_time);
  });
  if (result.has_value() && std::holds_alternative<duration_t>(result.value())) {
    return std::get<duration_t>(result.value());
  }
  return std::nullopt;

  //
  //
  // auto const duration_with_waiting = get_td_duration_split<SearchDir>(c, t);
  // if (duration_with_waiting.has_value()) {
  //   return duration_with_waiting->duration_ +
  //          duration_with_waiting->waiting_time_;
  // }
  // return std::nullopt;
}

template <typename Collection>
std::optional<duration_t> get_td_duration(direction const search_dir,
                                          Collection const& c,
                                          unixtime_t const t) {
  return search_dir == direction::kForward
             ? get_td_duration<direction::kForward>(c, t)
             : get_td_duration<direction::kBackward>(c, t);
}

template <direction SearchDir, typename Collection, typename Fn>
void for_each_footpath(Collection const& c, unixtime_t const t, Fn&& f) {
  utl::equal_ranges_linear(
      begin(c), end(c),
      [](td_footpath const& a, td_footpath const& b) {
        return a.target_ == b.target_;
      },
      [&](auto&& from, auto&& to) {
        auto const duration =
            get_td_duration<SearchDir>(std::span{from, to}, t);
        if (duration.has_value()) {
          f(footpath{from->target_, *duration});
        }
      });
}

template <typename Collection, typename Fn>
void for_each_footpath(direction const search_dir,
                       Collection const& c,
                       unixtime_t const t,
                       Fn&& f) {
  search_dir == direction::kForward
      ? for_each_footpath<direction::kForward>(c, t, std::forward<Fn>(f))
      : for_each_footpath<direction::kBackward>(c, t, std::forward<Fn>(f));
}

}  // namespace nigiri