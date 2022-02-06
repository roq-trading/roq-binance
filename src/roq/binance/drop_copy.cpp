/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/binance/drop_copy.h"

#include "roq/utils/mask.h"
#include "roq/utils/safe_cast.h"
#include "roq/utils/update.h"

#include "roq/core/metrics/factory.h"

#include "roq/binance/flags.h"

#include "roq/binance/json/utils.h"

using namespace std::literals;

namespace roq {
namespace binance {

namespace {
const auto NAME = "ex"sv;
const auto SUPPORTS = utils::Mask{
    SupportType::ORDER_ACK,
    SupportType::ORDER,
    SupportType::TRADE,
    SupportType::FUNDS,
};

auto create_query(const std::string_view &listen_key) {
  assert(!std::empty(listen_key));
  return fmt::format("?streams={}"sv, listen_key);
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
    : handler_(handler), stream_id_(stream_id),
      name_(fmt::format("{}:{}:{}"sv, stream_id_, NAME, security.get_account())),
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
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(name_, "parse"sv),
          .outbound_account_position = create_metrics(name_, "outbound_account_position"sv),
          .balance_update = create_metrics(name_, "balance_update"sv),
          .execution_report = create_metrics(name_, "execution_report"sv),
          .list_status = create_metrics(name_, "list_status"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
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
      .write(profile_.outbound_account_position, metrics::PROFILE)
      .write(profile_.balance_update, metrics::PROFILE)
      .write(profile_.execution_report, metrics::PROFILE)
      .write(profile_.list_status, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void DropCopy::operator()(const core::web::ClientSocket::Connected &) {
}

void DropCopy::operator()(const core::web::ClientSocket::Disconnected &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
}

void DropCopy::operator()(const core::web::ClientSocket::Ready &) {
  (*this)(ConnectionStatus::DOWNLOADING);
  download_.begin();
}

void DropCopy::operator()(const core::web::ClientSocket::Close &) {
}

void DropCopy::operator()(const core::web::ClientSocket::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .account = security_.get_account(),
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void DropCopy::operator()(const core::web::ClientSocket::Text &text) {
  parse(text.payload);
}

void DropCopy::operator()(const core::web::ClientSocket::Binary &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    auto trace_info = server::create_trace_info();
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = security_.get_account(),
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::WEB_SOCKET,
        .priority = Priority::PRIMARY,
    };
    log::info("stream_status={}"sv, stream_status);
    server::create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t DropCopy::download(DropCopyState state) {
  switch (state) {
    case DropCopyState::UNDEFINED:
      assert(false);
      break;
    case DropCopyState::DONE:
      (*this)(ConnectionStatus::READY);
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
      auto trace_info = server::create_trace_info();
      core::json::Buffer buffer(decode_buffer_);
      log::debug(R"(HERE message="{}")"sv, message);
      json::UserStreamParser::dispatch(*this, message, buffer, trace_info);
    } catch (...) {
      log::warn(R"(message="{}")"sv, message);
      core::tools::UnhandledException::terminate();
    }
  });
}

void DropCopy::operator()(const server::Trace<json::OutboundAccountPosition> &event) {
  profile_.outbound_account_position([&]() {
    auto &[trace_info, outbound_account_position] = event;
    log::info<2>("outbound_account_position={}"sv, outbound_account_position);
    for (auto &item : outbound_account_position.balances) {
      FundsUpdate funds_update{
          .stream_id = stream_id_,
          .account = security_.get_account(),
          .currency = item.asset,
          .balance = item.free_amount,
          .hold = item.locked_amount,
          .external_account = {},
      };
      create_trace_and_dispatch(handler_, trace_info, funds_update, true);
    }
  });
}

void DropCopy::operator()(const server::Trace<json::BalanceUpdate> &event) {
  profile_.balance_update([&]() {
    auto &[trace_info, balance_update] = event;
    log::info<2>("balance_update={}"sv, balance_update);
    // note! contains delta (changes) -- we're not going to use here
  });
}

void DropCopy::operator()(const server::Trace<json::ExecutionReport> &event) {
  profile_.execution_report([&]() {
    // auto &[trace_info, execution_report] = event;
    auto &trace_info = event.trace_info;
    auto &execution_report = event.value;
    log::info<2>("execution_report={}"sv, execution_report);
    auto side = json::map(execution_report.side);
    auto order_type = json::map(execution_report.order_type);
    auto time_in_force = json::map(execution_report.time_in_force);
    auto external_order_id = fmt::format("{}"sv, execution_report.order_id);
    auto status = json::map(execution_report.current_order_status);
    auto average_traded_price =
        utils::compare(execution_report.cumulative_filled_quantity, 0.0) == 0
            ? NaN
            : (execution_report.cumulative_quote_asset_transacted_quantity /
               execution_report.cumulative_filled_quantity);
    auto last_liquidity = execution_report.is_trade_maker ? Liquidity::MAKER : Liquidity::TAKER;
    oms::OrderUpdate order_update{
        .account = security_.get_account(),
        .exchange = Flags::exchange(),
        .symbol = execution_report.symbol,
        .side = side,
        .position_effect = {},
        .max_show_quantity = NaN,
        .order_type = order_type,
        .time_in_force = time_in_force,
        .execution_instruction = {},
        .order_template = {},
        .create_time_utc = {},
        .update_time_utc = execution_report.transaction_time,
        .external_account = {},
        .external_order_id = external_order_id,
        .status = status,
        .quantity = NaN,
        .price = execution_report.price,
        .stop_price = execution_report.stop_price,
        .remaining_quantity = NaN,
        .traded_quantity = execution_report.cumulative_filled_quantity,
        .average_traded_price = average_traded_price,
        .last_traded_quantity = execution_report.last_executed_quantity,
        .last_traded_price = execution_report.last_executed_price,
        .last_liquidity = last_liquidity,
    };
    if (shared_.update_order(
            execution_report.client_order_id,
            stream_id_,
            trace_info,
            order_update,
            [&](auto &order) {
              if (execution_report.current_execution_type == json::ExecutionType::TRADE) {
                auto external_trade_id =
                    fmt::format("{}"sv, execution_report.trade_id);  // XXX HANS
                Fill fill{
                    .external_trade_id = {},
                    .quantity = execution_report.last_executed_quantity,
                    .price = execution_report.last_executed_price,
                };
                TradeUpdate trade_update{
                    .stream_id = stream_id_,
                    .account = order.account,
                    .order_id = order.order_id,
                    .exchange = order.exchange,
                    .symbol = order.symbol,
                    .side = order.side,
                    .position_effect = order.position_effect,
                    .create_time_utc = utils::safe_cast(execution_report.transaction_time),
                    .update_time_utc = utils::safe_cast(execution_report.transaction_time),
                    .external_account = order.external_account,
                    .external_order_id = order.external_order_id,
                    .fills = {&fill, 1},
                    .routing_id = order.routing_id,
                };
                server::create_trace_and_dispatch(
                    handler_, trace_info, trade_update, true, order.user_id);
              }
            })) {
    } else {
      log::warn<1>("*** EXTERNAL ORDER ***"sv);
      log::warn<2>("execution_report={}"sv, execution_report);
    }
  });
}

void DropCopy::operator()(const server::Trace<json::ListStatus> &event) {
  profile_.list_status([&]() {
    auto &[trace_info, list_status] = event;
    log::info<2>("list_status={}"sv, list_status);
  });
}

}  // namespace binance
}  // namespace roq
