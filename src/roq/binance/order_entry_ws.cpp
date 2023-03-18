/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/binance/order_entry_ws.hpp"

#include "roq/mask.hpp"

#include "roq/utils/update.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/web/socket/client_factory.hpp"

#include "roq/binance/flags.hpp"

#include "roq/binance/json/utils.hpp"
#include "roq/binance/json/ws_api_type.hpp"

using namespace std::literals;

using namespace fmt::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const NAME = "ex"sv;

auto const SUPPORTS = Mask{
    SupportType::ORDER_ACK,
    SupportType::ORDER,
    SupportType::TRADE,
    SupportType::FUNDS,
};

auto const REQUEST_ID = uint32_t{1000000};
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id, auto const &account) {
  return fmt::format("{}:{}:{}"_cf, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &context) {
  auto uri = Flags::ws_api_uri();
  auto config = web::socket::Client::Config{
      .always_reconnect = true,
      .connection_timeout = server::Flags::net_connection_timeout(),
      .disconnect_on_idle_timeout = {},
      .validate_certificate = server::Flags::net_tls_validate_certificate(),
      .uris = {&uri, 1},
      .query = {},
      .ping_frequency = Flags::ws_api_ping_freq(),
      .read_buffer_size = Flags::decode_buffer_size(),
      .encode_buffer_size = Flags::encode_buffer_size(),
  };
  return web::socket::ClientFactory::create(handler, context, config, []() -> std::string { return {}; });
}

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(auto const &group, auto const &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

// === IMPLEMENTATION ===

OrderEntryWS::OrderEntryWS(
    Handler &handler,
    io::Context &context,
    uint16_t stream_id,
    Authenticator &authenticator,
    Shared &shared,
    Request &request)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, authenticator.get_account())},
      connection_{create_connection(*this, context)}, decode_buffer_{Flags::decode_buffer_size()},
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
      authenticator_{authenticator}, shared_{shared}, request_{request}, request_id_{REQUEST_ID} {
}

void OrderEntryWS::operator()(Event<Start> const &) {
  (*connection_).start();
}

void OrderEntryWS::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void OrderEntryWS::operator()(Event<Timer> const &event) {
  (*connection_).refresh(event.value.now);
  if (ready() && !downloading()) {
    if (!downloading() && request_.respond_account < request_.request_account) {
      log::info("Download account..."sv);
      get_account();
      download_account_ = true;
    }
    if (!downloading() && request_.respond_orders < request_.request_orders) {
      log::info("Download orders..."sv);
      get_open_orders();
      download_orders_ = true;
    }
  }
}

void OrderEntryWS::operator()(metrics::Writer &writer) {
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

void OrderEntryWS::operator()(Event<Disconnected> const &) {
}

uint16_t OrderEntryWS::operator()(Event<CreateOrder> const &, oms::Order const &, std::string_view const &request_id) {
  throw oms::NotSupported{"not supported"sv};
}

uint16_t OrderEntryWS::operator()(
    Event<ModifyOrder> const &,
    oms::Order const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  throw oms::NotSupported{"not supported"sv};
}

uint16_t OrderEntryWS::operator()(
    Event<CancelOrder> const &,
    oms::Order const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  throw oms::NotSupported{"not supported"sv};
}

uint16_t OrderEntryWS::operator()(Event<CancelAllOrders> const &, std::string_view const &request_id) {
  throw oms::NotSupported{"not supported"sv};
}

void OrderEntryWS::get_listen_key() {
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}-{}",)"
      R"("method":"userDataStream.start",)"
      R"("params":{{)"
      R"("apiKey":"{}")"
      R"(}})"
      R"(}})"sv,
      ++request_id_,
      magic_enum::enum_name(json::WSAPIType::LISTEN_KEY),
      authenticator_.get_key());
  log::debug("{}"sv, message);
  (*connection_).send_text(message);
}

void OrderEntryWS::get_account() {
  auto now = clock::get_realtime<std::chrono::milliseconds>();
  auto message_for_signature = fmt::format("apiKey={}&timestamp={}"sv, authenticator_.get_key(), now.count());
  auto signature = authenticator_.create_ws_api_signature(message_for_signature);
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}-{}",)"
      R"("method":"account.status",)"
      R"("params":{{)"
      R"("apiKey":"{}",)"
      R"("signature":"{}",)"
      R"("timestamp":"{}")"
      R"(}})"
      R"(}})"sv,
      ++request_id_,
      magic_enum::enum_name(json::WSAPIType::ACCOUNT_STATUS),
      authenticator_.get_key(),
      signature,
      now.count());
  log::debug("{}"sv, message);
  (*connection_).send_text(message);
}

void OrderEntryWS::get_open_orders() {
}

void OrderEntryWS::operator()(web::socket::Client::Connected const &) {
}

void OrderEntryWS::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
}

void OrderEntryWS::operator()(web::socket::Client::Ready const &) {
  get_listen_key();
  (*this)(ConnectionStatus::LOGIN_SENT);
}

void OrderEntryWS::operator()(web::socket::Client::Close const &) {
}

void OrderEntryWS::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = authenticator_.get_account(),
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntryWS::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
}

void OrderEntryWS::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntryWS::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    auto stream_status = StreamStatus{
        .stream_id = stream_id_,
        .account = authenticator_.get_account(),
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::WS,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
    };
    log::info("stream_status={}"sv, stream_status);
    create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

void OrderEntryWS::parse(std::string_view const &message) {
  profile_.parse([&]() {
    try {
      TraceInfo trace_info;
      core::json::Buffer buffer{decode_buffer_};
      log::debug(R"(HERE message="{}")"sv, message);
      json::WSAPIParser::dispatch(*this, message, buffer, trace_info);
    } catch (...) {
      log::warn(R"(message="{}")"sv, message);
      core::tools::UnhandledException::terminate();
    }
  });
}

void OrderEntryWS::operator()(Trace<json::Error> const &event) {
  auto &[trace_info, error] = event;
  log::debug("HERE {}"sv, error);
}

void OrderEntryWS::operator()(Trace<json::ListenKey> const &event) {
  auto &[trace_info, listen_key] = event;
  log::debug("HERE {}"sv, listen_key);
  auto listen_key_update = ListenKeyUpdate{
      .account = authenticator_.get_account(),
      .listen_key = listen_key.listen_key,
  };
  create_trace_and_dispatch(handler_, trace_info, listen_key_update);
  (*this)(ConnectionStatus::READY);
}

void OrderEntryWS::operator()(Trace<json::Account> const &event) {
  auto &[trace_info, account] = event;
  log::debug("HERE {}"sv, account);
  for (auto &item : account.balances) {
    // log::debug("item={}"sv, item);
    auto funds_update = FundsUpdate{
        .stream_id = stream_id_,
        .account = authenticator_.get_account(),
        .currency = item.asset,
        .balance = item.free,
        .hold = item.locked,
        .external_account = {},
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
  request_.respond_account = clock::get_system();  // completion
  download_account_ = false;
}

}  // namespace binance
}  // namespace roq
