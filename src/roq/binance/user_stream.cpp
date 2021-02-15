/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/user_stream.h"

#include <cassert>
#include <utility>

#include "roq/core/patterns.h"

#include "roq/core/clock.h"

#include "roq/core/charconv.h"

#include "roq/binance/flags.h"

using namespace roq::literals;

namespace roq {
namespace binance {

namespace {
static auto create_query(const std::string_view &listen_key) {
  return roq::format("?streams={}"_fmt, listen_key);
}

constexpr std::string_view CONNECTION = "user_stream"_sv;

static auto create_counter(const std::string_view &function) {
  return core::metrics::Counter(Flags::name(), CONNECTION, function);
}

static auto create_profile(const std::string_view &function) {
  return core::metrics::Profile(Flags::name(), CONNECTION, function);
}

static auto create_latency(const std::string_view &function) {
  return core::metrics::Latency(Flags::name(), CONNECTION, function);
}
}  // namespace

UserStream::UserStream(
    Handler &handler,
    Random &random,
    core::event::Base &base,
    core::event::DNSBase &dns_base,
    core::ssl::Context &ssl_context,
    const std::string_view &listen_key)
    : handler_(handler), query_(create_query(listen_key)), random_(random),
      connection_(
          *this,
          base,
          dns_base,
          ssl_context,
          core::URI(Flags::ws_uri()),
          query_,
          std::chrono::seconds{Flags::ws_ping_freq_secs()},
          Flags::decode_buffer_size(),
          Flags::encode_buffer_size(),
          []() { return std::string(); }),
      decode_buffer_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_counter("disconnect"_sv),
      },
      profile_{
          .parse = create_profile("parse"_sv),
          .outbound_account_info = create_profile("outbound_account_info"_sv),
          .outbound_account_position = create_profile("outbound_account_position"_sv),
          .balance_update = create_profile("balance_update"_sv),
          .execution_report = create_profile("execution_report"_sv),
      },
      latency_{
          .ping = create_latency("ping"_sv),
          .heartbeat = create_latency("heartbeat"_sv),
      } {
}

bool UserStream::ready() const {
  return connection_.ready();
}

void UserStream::operator()(const Event<Start> &) {
  connection_.start();
}

void UserStream::operator()(const Event<Stop> &) {
  connection_.stop();
}

void UserStream::operator()(const Event<Timer> &event) {
  connection_.refresh(event.value.now);
}

void UserStream::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.parse, metrics::PROFILE)
      .write(profile_.outbound_account_info, metrics::PROFILE)
      .write(profile_.outbound_account_position, metrics::PROFILE)
      .write(profile_.balance_update, metrics::PROFILE)
      .write(profile_.execution_report, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void UserStream::operator()(const core::web::Socket::Connected &) {
}

void UserStream::operator()(const core::web::Socket::Disconnected &) {
  ++counter_.disconnect;
}

void UserStream::operator()(const core::web::Socket::Ready &) {
  LOG(INFO)("Ready"_sv);
}

void UserStream::operator()(const core::web::Socket::Close &) {
}

void UserStream::operator()(const core::web::Socket::Latency &latency) {
  server::TraceInfo trace_info;
  ExternalLatency external_latency{
      .name = CONNECTION,
      .latency = latency.sample,
  };
  handler_(external_latency, trace_info);
  latency_.ping.update(latency.sample);
}

void UserStream::operator()(const core::web::Socket::Text &text) {
  parse(text.payload);
}

void UserStream::parse(const std::string_view &message) {
  profile_.parse([&]() {
    try {
      server::TraceInfo trace_info;
      core::json::Buffer buffer(decode_buffer_);
      json::UserStreamParser::dispatch(*this, message, buffer, trace_info);
    } catch (std::exception &e) {
      LOG(WARNING)(R"(message="{}")"_sv, message);
      LOG(FATAL)(R"(ERROR what="{}")"_sv, e.what());
    }
  });
}

void UserStream::operator()(
    const json::OutboundAccountInfo &outbound_account_info, const server::TraceInfo &trace_info) {
  profile_.outbound_account_info([&]() {
    VLOG(3)(R"(outbound_account_info={})"_sv, outbound_account_info);
    handler_(outbound_account_info, trace_info);
  });
}

void UserStream::operator()(
    const json::OutboundAccountPosition &outbound_account_position,
    const server::TraceInfo &trace_info) {
  profile_.outbound_account_position([&]() {
    VLOG(3)(R"(outbound_account_position={})"_sv, outbound_account_position);
    handler_(outbound_account_position, trace_info);
  });
}

void UserStream::operator()(
    const json::BalanceUpdate &balance_update, const server::TraceInfo &trace_info) {
  profile_.balance_update([&]() {
    VLOG(3)(R"(balance_update={})"_sv, balance_update);
    handler_(balance_update, trace_info);
  });
}

void UserStream::operator()(
    const json::ExecutionReport &execution_report, const server::TraceInfo &trace_info) {
  profile_.execution_report([&]() {
    VLOG(3)(R"(execution_report={})"_sv, execution_report);
    handler_(execution_report, trace_info);
  });
}

}  // namespace binance
}  // namespace roq
