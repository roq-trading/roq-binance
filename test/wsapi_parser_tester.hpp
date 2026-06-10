/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/binance/protocol/json/wsapi_parser.hpp"

namespace roq {
namespace binance {

template <typename T>
struct WSAPIParserTester final : public protocol::json::WSAPIParser::Handler {
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
    WSAPIParserTester handler{callback};
    auto res = protocol::json::WSAPIParser::dispatch(handler, message, buffers, {}, false);
    CHECK(res == true);
    CHECK(handler.found_ == true);
  }

 protected:
  explicit WSAPIParserTester(callback_type const &callback) : callback_{callback} {}

  void operator()(Trace<protocol::json::WSAPISessionLogon> const &event) override { dispatch(event); }

  void operator()(Trace<protocol::json::WSAPIUserDataStreamSubscribe> const &event) override { dispatch(event); }
  void operator()(Trace<protocol::json::WSAPIEventStreamTerminated> const &event) override { dispatch(event); }

  void operator()(Trace<protocol::json::WSAPIAccount> const &event) override { dispatch(event); }
  void operator()(Trace<protocol::json::WSAPIOpenOrders> const &event) override { dispatch(event); }
  void operator()(Trace<protocol::json::WSAPITrades> const &event) override { dispatch(event); }

  void operator()(Trace<protocol::json::WSAPIOrderPlace> const &event, protocol::json::WSAPIRequest const &) override { dispatch(event); }
  void operator()(Trace<protocol::json::WSAPIOrderAmendKeepPriority> const &event, protocol::json::WSAPIRequest const &) override { dispatch(event); }
  void operator()(Trace<protocol::json::WSAPICancelOrder> const &event, protocol::json::WSAPIRequest const &) override { dispatch(event); }
  void operator()(Trace<protocol::json::WSAPICancelOpenOrders> const &event, protocol::json::WSAPIRequest const &) override { dispatch(event); }

  void operator()(Trace<protocol::json::WSAPIOutboundAccountPosition> const &event) override { dispatch(event); }
  void operator()(Trace<protocol::json::WSAPIBalanceUpdate> const &event) override { dispatch(event); }
  void operator()(Trace<protocol::json::WSAPIExecutionReport> const &event) override { dispatch(event); }

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
