/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/user_stream.h"

#include <fmt/format.h>

#include <cassert>
#include <utility>

#include "roq/patterns.h"

#include "roq/core/clock.h"

#include "roq/core/charconv.h"

#include "roq/binance/options.h"

namespace roq {
namespace binance {

namespace {
static auto create_query(const std::string_view& listen_key) {
  return fmt::format(
      FMT_STRING("?streams={}"),
      listen_key);
}

constexpr std::string_view CONNECTION = "user_stream";

static auto create_counter(
    const std::string_view& function) {
  return core::metrics::Counter(
      FLAGS_name,
      CONNECTION,
      function);
}

static auto create_profile(
    const std::string_view& function) {
  return core::metrics::Profile(
      FLAGS_name,
      CONNECTION,
      function);
}

static auto create_latency(
    const std::string_view& function) {
  return core::metrics::Latency(
      FLAGS_name,
      CONNECTION,
      function);
}
}  // namespace

UserStream::UserStream(
    Handler& handler,
    Random& random,
    core::event::Base& base,
    core::event::DNSBase& dns_base,
    core::ssl::Context& ssl_context,
    const std::string_view& listen_key)
    : _handler(handler),
      _query(create_query(listen_key)),
      _random(random),
      _connection(
          *this,
          base,
          dns_base,
          ssl_context,
          core::URI(FLAGS_ws_uri),
          _query,
          std::chrono::seconds { FLAGS_ws_ping_freq_secs },
          FLAGS_decode_buffer_size,
          FLAGS_encode_buffer_size,
          []() { return std::string(); }),
      _decode_buffer(FLAGS_decode_buffer_size),
      _counter {
        .disconnect = create_counter("disconnect"),
      },
      _profile {
        .parse = create_profile("parse"),
        .outbound_account_info = create_profile("outbound_account_info"),
        .outbound_account_position = create_profile("outbound_account_position"),
        .balance_update = create_profile("balance_update"),
        .execution_report = create_profile("execution_report"),
      },
      _latency {
        .ping = create_latency("ping"),
        .heartbeat = create_latency("heartbeat"),
      } {
}

bool UserStream::ready() const {
  return _connection.ready();
}

void UserStream::operator()(const server::StartEvent&) {
  _connection.start();
}

void UserStream::operator()(const server::StopEvent&) {
  _connection.stop();
}

void UserStream::operator()(const server::TimerEvent& event) {
  _connection.refresh(event.now);
}

void UserStream::operator()(metrics::Writer& writer) {
  writer
    // counter
    .write(_counter.disconnect, metrics::COUNTER)
    // profile
    .write(_profile.parse, metrics::PROFILE)
    .write(_profile.outbound_account_info, metrics::PROFILE)
    .write(_profile.outbound_account_position, metrics::PROFILE)
    .write(_profile.balance_update, metrics::PROFILE)
    .write(_profile.execution_report, metrics::PROFILE)
    // latency
    .write(_latency.ping, metrics::LATENCY)
    .write(_latency.heartbeat, metrics::LATENCY);
}

void UserStream::operator()(const core::web::Socket::Connected&) {
}

void UserStream::operator()(const core::web::Socket::Disconnected&) {
  ++_counter.disconnect;
}

void UserStream::operator()(const core::web::Socket::Ready&) {
  LOG(INFO)("Ready");
}

void UserStream::operator()(const core::web::Socket::Close&) {
}

void UserStream::operator()(const core::web::Socket::Latency& latency) {
  _latency.ping.update(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          latency.sample).count());
}

void UserStream::operator()(const core::web::Socket::Text& text) {
  parse(text.payload);
}

void UserStream::parse(const std::string_view& message) {
  _profile.parse(
      [&]() {
        try {
          core::json::Buffer buffer(_decode_buffer);
          json::UserStreamParser::dispatch(
              *this,
              message,
              buffer);
        } catch (std::exception& e) {
          LOG(WARNING)(
              FMT_STRING(R"(message="{}")"),
              message);
          LOG(FATAL)(
              FMT_STRING(R"(ERROR what="{}")"),
              e.what());
        }
      });
}

void UserStream::operator()(
    const json::OutboundAccountInfo& outbound_account_info) {
  _profile.outbound_account_info(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(outbound_account_info={})"),
            outbound_account_info);
        _handler(outbound_account_info);
      });
}

void UserStream::operator()(
    const json::OutboundAccountPosition& outbound_account_position) {
  _profile.outbound_account_position(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(outbound_account_position={})"),
            outbound_account_position);
        _handler(outbound_account_position);
      });
}

void UserStream::operator()(const json::BalanceUpdate& balance_update) {
  _profile.balance_update(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(balance_update={})"),
            balance_update);
        _handler(balance_update);
      });
}

void UserStream::operator()(const json::ExecutionReport& execution_report) {
  _profile.execution_report(
      [&]() {
        VLOG(3)(
            FMT_STRING(R"(execution_report={})"),
            execution_report);
        _handler(execution_report);
      });
}

}  // namespace binance
}  // namespace roq
