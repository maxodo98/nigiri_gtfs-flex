#include "nigiri/loader/gtfs/booking_rule.h"

#include <nigiri/loader/gtfs/parse_time.h>
#include <nigiri/loader/gtfs/services.h>
#include <nigiri/logging.h>
#include <utl/parser/buf_reader.h>
#include <utl/parser/csv_range.h>
#include <utl/parser/line_range.h>
#include <utl/pipes/transform.h>
#include <utl/pipes/vec.h>
#include <utl/progress_tracker.h>
#include "nigiri/timetable.h"

namespace nigiri::loader::gtfs {
booking_rule_map_t read_booking_rules(traffic_days_t const& services,
                                      timetable& tt,
                                      std::string_view file_content) {
  auto const timer = scoped_timer{"gtfs_flex.loader.booking_rules"};

  auto const progress_tracker = utl::get_active_progress_tracker();
  progress_tracker->status("Parse td_Booking_Rules")
      .out_bounds(0.F, 1.F)
      .in_high(file_content.size());

  struct csv_booking_rule {
    utl::csv_col<utl::cstr, UTL_NAME("booking_rule_id")> id_;
    utl::csv_col<uint8_t, UTL_NAME("booking_type")> type_;
    utl::csv_col<uint16_t, UTL_NAME("prior_notice_duration_min")>
        prior_notice_duration_min_;
    utl::csv_col<uint16_t, UTL_NAME("prior_notice_duration_max")>
        prior_notice_duration_max_;
    utl::csv_col<uint16_t, UTL_NAME("prior_notice_last_day")>
        prior_notice_last_day_;
    utl::csv_col<utl::cstr, UTL_NAME("prior_notice_last_time")>
        prior_notice_last_time_;
    utl::csv_col<utl::cstr, UTL_NAME("prior_notice_start_day")>
        prior_notice_start_day_;
    utl::csv_col<utl::cstr, UTL_NAME("prior_notice_start_time")>
        prior_notice_start_time_;
    utl::csv_col<utl::cstr, UTL_NAME("prior_notice_service_id")>
        prior_notice_service_id_;
    utl::csv_col<utl::cstr, UTL_NAME("message")> message_;
    utl::csv_col<utl::cstr, UTL_NAME("pickup_message")> pickup_message_;
    utl::csv_col<utl::cstr, UTL_NAME("drop_off_message")> drop_off_message_;
    utl::csv_col<utl::cstr, UTL_NAME("phone_number")> phone_number_;
    utl::csv_col<utl::cstr, UTL_NAME("info_url")> info_url_;
    utl::csv_col<utl::cstr, UTL_NAME("booking_url")> booking_url_;
  };

  return utl::line_range{utl::make_buf_reader(
             file_content, progress_tracker->update_fn())}  //
         | utl::csv<csv_booking_rule>()  //
         |
         utl::transform([&](csv_booking_rule const& b) {
           // Checking GTFS-flex-specification requirements
           assert(!b.id_->view().empty());
           assert(b.type_.val() == kRealTimeBooking ||
                  b.type_.val() == kSameDayBooking ||
                  b.type_.val() == kPriorDaysBooking);
           switch (b.type_.val()) {
             case kRealTimeBooking: break;
             case kSameDayBooking:
               assert(b.prior_notice_duration_min_.val() > 0);
               break;
             case kPriorDaysBooking:
               assert(b.prior_notice_last_day_.val() > 0);
               assert(!b.prior_notice_last_time_->view().empty());
               if (!b.prior_notice_start_day_->view().empty()) {
                 assert(!b.prior_notice_start_time_->view().empty());
               }
               break;
           }

           auto traffic_days_it = services.end();
           auto error = false;
           if (!b.prior_notice_service_id_->empty()) {
             traffic_days_it =
                 services.find(b.prior_notice_service_id_->view());
             if (traffic_days_it == end(services)) {
               log(log_lvl::error, "loader.gtfs.booking_rule",
                   R"(booking_rule "{}": prior_notice_service_id "{}" not found)",
                   b.id_->view(), b.prior_notice_service_id_->view());
               error = true;
             }
           }

           return std::pair{
               b.id_->to_str(),
               tt.register_booking_rule(
                   {.type_ = b.type_.val(),
                    .prior_notice_duration_min_ =
                        b.prior_notice_duration_min_.val(),
                    .prior_notice_duration_max_ =
                        b.prior_notice_duration_max_.val(),
                    .prior_notice_last_day_ = b.prior_notice_last_day_.val(),
                    .prior_notice_last_time_ =
                        hhmm_to_min(*b.prior_notice_last_time_),
                    .prior_notice_start_day_ =
                        b.prior_notice_start_day_->empty()
                            ? static_cast<uint16_t>(0)
                            : static_cast<uint16_t>(
                                  strtoul(b.prior_notice_start_day_->c_str(),
                                          NULL, 10)),
                    .prior_notice_start_time_ =
                        hhmm_to_min(*b.prior_notice_start_time_),
                    .prior_notice_service_id_ =
                        b.prior_notice_service_id_->empty() || error
                            ? std::nullopt
                            : traffic_days_it,
                    .message_ = b.message_->to_str(),
                    .pickup_message_ = b.pickup_message_->to_str(),
                    .drop_off_message_ = b.drop_off_message_->to_str(),
                    .phone_number_ = b.phone_number_->to_str(),
                    .info_url_ = b.info_url_->to_str(),
                    .booking_url_ = b.booking_url_->to_str()})};
         })  //
         | utl::to<booking_rule_map_t>();
}

}  // namespace nigiri::loader::gtfs