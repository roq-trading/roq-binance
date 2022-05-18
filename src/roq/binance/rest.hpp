/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_set.h>

#include <string>
#include <string_view>
#include <vector>

#include "roq/core/download.hpp"

#include "roq/core/metrics/counter.hpp"
#include "roq/core/metrics/latency.hpp"
#include "roq/core/metrics/profile.hpp"

#include "roq/core/io/context.hpp"

#include "roq/core/web/client.hpp"

#include "roq/server.hpp"

#include "roq/binance/rest_state.hpp"
#include "roq/binance/shared.hpp"

#include "roq/binance/json/depth.hpp"
#include "roq/binance/json/exchange_info.hpp"

namespace roq {
namespace binance {

class Rest final : public core::web::Client::Handler {
 public:
  struct SymbolsUpdate final {
    std::vector<Symbol> &symbols;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus const> const &) = 0;
    virtual void operator()(Trace<ExternalLatency const> const &) = 0;
    virtual void operator()(Trace<ReferenceData const> const &, bool is_last) = 0;
    virtual void operator()(Trace<MarketStatus const> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(SymbolsUpdate &) = 0;
  };

  Rest(Handler &, core::io::Context &, uint16_t stream_id, Shared &);

  Rest(Rest &&) = delete;
  Rest(Rest const &) = delete;

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &);

 protected:
  void operator()(core::web::Client::Connected const &);
  void operator()(core::web::Client::Disconnected const &);
  void operator()(core::web::Client::Latency const &);

  void operator()(ConnectionStatus);

  uint32_t download(RestState state);

  void get_exchange_info();
  void get_exchange_info_ack(Trace<core::web::Response const> const &, uint32_t sequence);
  void operator()(Trace<json::ExchangeInfo const> const &);

  void get_depth(std::string_view const &symbol);
  void get_depth_ack(Trace<core::web::Response const> const &, std::string_view const &symbol);
  void operator()(Trace<json::Depth const> const &, std::string_view const &symbol);

  void check_request_queue(std::chrono::nanoseconds now);

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  // connection
  core::web::Client connection_;
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
