/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/binance/drop_copy.h"

#include "roq/mask.h"
#include "roq/update.h"

#include "roq/core/metrics/factory.h"

#include "roq/binance/flags.h"

#include "roq/binance/json/utils.h"

using namespace roq::literals;

namespace roq {
namespace binance {

namespace {
static const auto NAME = "ex"_sv;
static const auto SUPPORTS = Mask{
    SupportType::FUNDS,
};

static auto create_query(const std::string_view &listen_key) {
  assert(!listen_key.empty());
  return roq::format("?streams={}"_fmt, listen_key);
}

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

DropCopy::DropCopy(
    Handler &handler,
    core::io::Context &context,
    uint16_t stream_id,
    Security &security,
    Shared &shared,
    const std::string_view &listen_key)
    : handler_(handler), stream_id_(stream_id), name_(roq::format("{}:{}"_fmt, stream_id_, NAME)),
      connection_(
          *this,
          context,
          core::URI(Flags::ws_uri()),
          create_query(listen_key),
          Flags::ws_ping_freq(),
          Flags::decode_buffer_size(),
          Flags::encode_buffer_size(),
          []() { return std::string(); }),
      decode_buffer_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_metrics(name_, "disconnect"_sv),
      },
      profile_{
          .parse = create_metrics(name_, "parse"_sv),
          .outbound_account_info = create_metrics(name_, "outbound_account_info"_sv),
          .outbound_account_position = create_metrics(name_, "outbound_account_position"_sv),
          .balance_update = create_metrics(name_, "balance_update"_sv),
          .execution_report = create_metrics(name_, "execution_report"_sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"_sv),
          .heartbeat = create_metrics(name_, "heartbeat"_sv),
      },
      security_(security), shared_(shared),
      download_({}, [this](auto state) { return download(state); }) {
}

bool DropCopy::ready() const {
  return connection_.ready();
}

void DropCopy::operator()(const Event<Start> &) {
  connection_.start();
}

void DropCopy::operator()(const Event<Stop> &) {
  connection_.stop();
}

void DropCopy::operator()(const Event<Timer> &event) {
  connection_.refresh(event.value.now);
}

void DropCopy::operator()(metrics::Writer &writer) {
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

void DropCopy::operator()(const core::web::Socket::Connected &) {
}

void DropCopy::operator()(const core::web::Socket::Disconnected &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(GatewayStatus::DISCONNECTED);
  download_.reset();
}

void DropCopy::operator()(const core::web::Socket::Ready &) {
  (*this)(GatewayStatus::DOWNLOADING);
  download_.begin();
}

void DropCopy::operator()(const core::web::Socket::Close &) {
}

void DropCopy::operator()(const core::web::Socket::Latency &latency) {
  server::TraceInfo trace_info;
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(trace_info, external_latency, handler_);
  latency_.ping.update(latency.sample);
}

void DropCopy::operator()(const core::web::Socket::Text &text) {
  parse(text.payload);
}

void DropCopy::operator()(GatewayStatus status) {
  if (update(status_, status)) {
    server::TraceInfo trace_info;
    StreamUpdate stream_update{
        .stream_id = stream_id_,
        .type = StreamType::WEB_SOCKET,
        .supports = SUPPORTS.get(),
        .account = security_.get_account(),
        .priority = Priority::PRIMARY,
        .status = status_,
    };
    log::info("stream_update={}"_fmt, stream_update);
    server::create_trace_and_dispatch(trace_info, stream_update, handler_);
  }
}

uint32_t DropCopy::download(DropCopyState state) {
  switch (state) {
    case DropCopyState::UNDEFINED:
      assert(false);
      break;
    case DropCopyState::DONE:
      (*this)(GatewayStatus::READY);
      assert(!ready_);
      ready_ = true;
      // subscribe(symbols_);
      return {};
  }
  assert(false);
  return {};
}

void DropCopy::parse(const std::string_view &message) {
  profile_.parse([&]() {
    try {
      server::TraceInfo trace_info;
      core::json::Buffer buffer(decode_buffer_);
      json::UserStreamParser::dispatch(*this, message, buffer, trace_info);
    } catch (std::exception &e) {
      log::warn(R"(message="{}")"_fmt, message);
      log::fatal(R"(ERROR what="{}")"_fmt, e.what());
    }
  });
}

void DropCopy::operator()(
    const json::OutboundAccountInfo &outbound_account_info, const server::TraceInfo &trace_info) {
  profile_.outbound_account_info([&]() {
    log::trace_3("outbound_account_info={}"_fmt, outbound_account_info);
    for (auto &item : outbound_account_info.balances) {
      FundsUpdate funds_update{
          .stream_id = stream_id_,
          .account = security_.get_account(),
          .currency = item.asset,
          .balance = item.free_amount,
          .hold = item.locked_amount,
          .external_account = {},
      };
      create_trace_and_dispatch(trace_info, funds_update, handler_, true);
    }
  });
}

void DropCopy::operator()(
    const json::OutboundAccountPosition &outbound_account_position,
    const server::TraceInfo &trace_info) {
  profile_.outbound_account_position([&]() {
    log::trace_3("outbound_account_position={}"_fmt, outbound_account_position);
    for (auto &item : outbound_account_position.balances) {
      FundsUpdate funds_update{
          .stream_id = stream_id_,
          .account = security_.get_account(),
          .currency = item.asset,
          .balance = item.free_amount,
          .hold = item.locked_amount,
          .external_account = {},
      };
      create_trace_and_dispatch(trace_info, funds_update, handler_, true);
    }
  });
}

void DropCopy::operator()(const json::BalanceUpdate &balance_update, const server::TraceInfo &) {
  profile_.balance_update([&]() {
    log::trace_3("balance_update={}"_fmt, balance_update);
    // note! contains delta (changes) -- we're not going to use here
  });
}

void DropCopy::operator()(
    const json::ExecutionReport &execution_report, const server::TraceInfo &trace_info) {
  profile_.execution_report([&]() {
    log::trace_3("execution_report={}"_fmt, execution_report);
    auto side = json::map(execution_report.side);
    auto status = json::map(execution_report.current_order_status);
    server::OMS_Lookup order_lookup{
        .symbol = execution_report.symbol,
        .side = side,
        .status = status,
        .price = execution_report.price,
        .remaining_quantity = NaN,
        .traded_quantity = execution_report.cumulative_filled_quantity,
        .timestamp = execution_report.transaction_time,  // XXX transact_time?
        .external_account = {},
        .external_order_id = execution_report.client_order_id,
    };
    auto found = shared_.find_order(
        execution_report.original_client_order_id,
        execution_report.client_order_id,
        order_lookup,
        trace_info,
        [&]([[maybe_unused]] const auto &order, [[maybe_unused]] auto &result) {
          // XXX IMPLEMENT
        });
    if (!found) {
      log::warn("*** EXTERNAL ORDER ***"_sv);
      log::warn("execution_report={}"_fmt, execution_report);
    }
  });
}

}  // namespace binance
}  // namespace roq
