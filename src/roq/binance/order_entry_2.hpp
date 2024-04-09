/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/gauge.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/core/download.hpp"

#include "roq/server.hpp"

#include "roq/binance/account.hpp"
#include "roq/binance/order_entry_state.hpp"
#include "roq/binance/request.hpp"
#include "roq/binance/shared.hpp"

#include "roq/binance/json/account.hpp"
#include "roq/binance/json/listen_key.hpp"

namespace roq {
namespace binance {

struct OrderEntry2 final : public web::rest::Client::Handler {
  struct ListenKeyUpdate final {
    std::string_view account;
    std::string_view listen_key;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(Trace<RateLimitsUpdate> const &) = 0;
    virtual void operator()(
        Trace<TradeUpdate> const &, bool is_last, uint8_t user_id, std::string_view const &request_id) = 0;
    virtual void operator()(Trace<FundsUpdate> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(ListenKeyUpdate const &) = 0;
  };

  OrderEntry2(Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &);

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &);

  void operator()(Event<Disconnected> const &);

 protected:
  bool downloading() const { return download_account_; }

  // web::rest::Client::Handler
  void operator()(Trace<web::rest::Client::Connected> const &) override;
  void operator()(Trace<web::rest::Client::Disconnected> const &) override;
  void operator()(Trace<web::rest::Client::Header> const &) override;
  void operator()(Trace<web::rest::Client::Latency> const &) override;

  void operator()(ConnectionStatus);

  uint32_t download(OrderEntryState state);

  void get_listen_key();
  void get_listen_key_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::ListenKey> const &);

  void get_account();
  void get_account_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::Account> const &);

  void refresh_listen_key(std::chrono::nanoseconds now);

  template <typename SuccessHandler, typename ErrorHandler>
  void process_response(web::rest::Response const &, SuccessHandler, ErrorHandler);

  template <typename Parse, typename Callback>
  void dispatch_error_2(web::rest::Response const &, web::http::Category, web::http::Status, Parse, Callback);  // XXX

  void test(web::http::Status);  // XXX
  void waf_limit_violation();

 private:
  Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  // connection
  std::unique_ptr<web::rest::Client> connection_;
  // buffers
  std::vector<std::byte> decode_buffer_;
  // metrics
  struct {
    utils::metrics::Counter disconnect;
  } counter_;
  struct {
    utils::metrics::Profile listen_key, listen_key_ack,  //
        account, account_ack;
  } profile_;
  struct {
    utils::metrics::Latency ping;
  } latency_;
  struct {
    utils::metrics::Gauge requests_1m;
  } rate_limiter_;
  // authentication
  Account &account_;
  // shared
  Shared &shared_;
  Request &request_;
  std::string listen_key_;
  // state
  bool ready_ = false;
  std::chrono::nanoseconds listen_key_refresh_ = {};
  ConnectionStatus status_ = {};
  core::Download<OrderEntryState> download_;
  // experimental
  bool download_account_ = false;
  std::vector<char> encode_buffer_;
};

}  // namespace binance
}  // namespace roq
