/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_set.h>

#include <string>
#include <string_view>
#include <vector>

#include "roq/core/download.hpp"

#include "roq/core/metrics/counter.hpp"
#include "roq/core/metrics/latency.hpp"
#include "roq/core/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/server.hpp"

#include "roq/binance/rest_state.hpp"
#include "roq/binance/shared.hpp"

#include "roq/binance/json/depth.hpp"
#include "roq/binance/json/exchange_info.hpp"

namespace roq {
namespace binance {

class Rest final : public web::rest::Client::Handler {
 public:
  struct SymbolsUpdate final {
    std::vector<Symbol> &symbols;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(Trace<ReferenceData> const &, bool is_last) = 0;
    virtual void operator()(Trace<MarketStatus> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(SymbolsUpdate &) = 0;
  };

  Rest(Handler &, io::Context &, uint16_t stream_id, Shared &);

  Rest(Rest &&) = delete;
  Rest(Rest const &) = delete;

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &);

 protected:
  void operator()(web::rest::Client::Connected const &);
  void operator()(web::rest::Client::Disconnected const &);
  void operator()(web::rest::Client::Latency const &);

  void operator()(ConnectionStatus);

  uint32_t download(RestState state);

  void get_exchange_info();
  void get_exchange_info_ack(Trace<web::rest::Response> const &, uint32_t sequence);
  void operator()(Trace<json::ExchangeInfo> const &);

  void get_depth(std::string_view const &symbol);
  void get_depth_ack(Trace<web::rest::Response> const &, std::string_view const &symbol);
  void operator()(Trace<json::Depth> const &, std::string_view const &symbol);

  void check_request_queue(std::chrono::nanoseconds now);

  template <typename SuccessHandler, typename ErrorHandler>
  void process_response(web::rest::Response const &, SuccessHandler, ErrorHandler);

  void test(web::http::Status);  // XXX
  void waf_limit_violation();

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  // connection
  std::unique_ptr<web::rest::Client> connection_;
  // buffers
  core::Buffer decode_buffer_;
  core::Buffer decode_buffer_2_;  // note! decode nested arrays (ExchangeInfo)
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } counter_;
  struct {
    core::metrics::Profile exchange_info, exchange_info_ack, depth, depth_ack;
  } profile_;
  struct {
    core::metrics::Latency ping;
  } latency_;
  // cache
  Shared &shared_;
  absl::flat_hash_set<Symbol> all_symbols_;
  // state
  bool ready_ = false;
  ConnectionStatus status_ = {};
  core::Download<RestState> download_;
};

}  // namespace binance
}  // namespace roq
