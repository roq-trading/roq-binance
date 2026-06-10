/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/binance/protocol/json/market_stream_parser.hpp"

namespace roq {
namespace binance {

template <typename T>
struct MarketStreamParserTester final : public protocol::json::MarketStreamParser::Handler {
  using value_type = std::remove_cvref_t<T>;
  using callback_type = std::function<void(value_type const &)>;

  static void dispatch(callback_type const &callback, std::string_view const &message, size_t buffer_size, size_t max_depth) {
    core::json::BufferStack buffers{buffer_size, max_depth};
    // simple
    // XXX FIXME TODO catch2 block ???
    T obj{message, buffers};
    callback(obj);
    // parser
    // XXX FIXME TODO catch2 block ???
    MarketStreamParserTester handler{callback};
    auto res = protocol::json::MarketStreamParser::dispatch(handler, message, buffers, {}, false);
    CHECK(res == true);
    CHECK(handler.found_ == true);
  }

 protected:
  explicit MarketStreamParserTester(callback_type const &callback) : callback_{callback} {}

  void operator()(Trace<protocol::json::Error> const &event, [[maybe_unused]] int64_t id) override { dispatch(event); }
  void operator()(Trace<protocol::json::Result> const &event, [[maybe_unused]] int64_t id) override { dispatch(event); }
  void operator()(Trace<protocol::json::AggTrade> const &event) override { dispatch(event); }
  void operator()(Trace<protocol::json::Trade> const &event) override { dispatch(event); }
  void operator()(Trace<protocol::json::MiniTicker> const &event) override { dispatch(event); }
  void operator()(Trace<protocol::json::BookTicker> const &event) override { dispatch(event); }
  void operator()(Trace<protocol::json::Depth> const &event, [[maybe_unused]] std::string_view const &symbol) override { dispatch(event); }
  void operator()(Trace<protocol::json::DepthUpdate> const &event) override { dispatch(event); }

  template <typename U>
  void dispatch(Trace<U> const &event) {
    if constexpr (std::is_invocable_v<callback_type, U>) {
      found_ = true;
      callback_(event);
    } else {
      FAIL();
    }
  }

 private:
  callback_type const callback_;
  bool found_ = false;
};

}  // namespace binance
}  // namespace roq
