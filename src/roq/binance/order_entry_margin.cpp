/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/order_entry_margin.hpp"

#include <tuple>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/compare.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/utils/charconv/from_chars.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/binance/json/encoder.hpp"
#include "roq/binance/json/error.hpp"
#include "roq/binance/json/map.hpp"
#include "roq/binance/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const NAME = "om"sv;

auto const SUPPORTS = Mask{
    SupportType::CREATE_ORDER,
    SupportType::CANCEL_ORDER,
    SupportType::ORDER_ACK,
    SupportType::TRADE,
    SupportType::FUNDS,
};

size_t const MAX_DECODE_BUFFER_DEPTH = 1;

size_t const DOWNLOAD_TRADES_LIMIT = 1000;
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id, auto &account) {
  return fmt::format("{}:{}:{}"sv, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &settings, auto &context) {
  auto uri = settings.rest.uri;
  auto config = web::rest::Client::Config{
      // connection
      .interface = {},
      .proxy = settings.rest.proxy,
      .uris = {&uri, 1},
      .host = settings.rest.host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = {},
      .disconnect_on_idle_timeout = {},
      .connection = web::http::Connection::KEEP_ALIVE,
      // request
      .allow_pipelining = true,
      .request_timeout = settings.rest.request_timeout,
      // response
      .suspend_on_retry_after = true,
      // http
      .query = {},
      .user_agent = ROQ_PACKAGE_NAME,
      .ping_frequency = settings.rest.ping_freq,
      .ping_path = settings.rest.ping_path,
      // implementation
      .decode_buffer_size = settings.misc.decode_buffer_size,
      .encode_buffer_size = settings.misc.encode_buffer_size,
  };
  return web::rest::Client::create(handler, context, config);
}

struct create_metrics final : public utils::metrics::Factory {
  create_metrics(auto &settings, auto &group, auto const &function) : utils::metrics::Factory{settings.app.name, group, function} {}
  create_metrics(auto &settings, auto &group, auto const &function, auto const &params) : utils::metrics::Factory{settings.app.name, group, function, params} {}
};

auto get_download_trades_lookback(auto &settings, auto download_trades_is_first) {
  if (download_trades_is_first) {
    if (settings.download.trades_lookback_on_restart.count()) {
      return settings.download.trades_lookback_on_restart;
    }
  }
  return settings.download.trades_lookback;
}

auto get_retry_after(auto &response) {
  std::chrono::nanoseconds result = {};
  response.dispatch(web::http::Header::RETRY_AFTER, [&](auto &value) {
    try {
      // XXX FIXME could also be a datetime (see https://datatracker.ietf.org/doc/html/rfc7231)
      auto seconds = utils::charconv::from_string_relaxed<int64_t>(value);
      result = std::chrono::seconds{seconds};
    } catch (RuntimeError &) {
      log::warn<5>(R"(Failed to parse text="{}")"sv, value);
    }
  });
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

OrderEntryMargin::OrderEntryMargin(OrderEntry::Handler &handler, io::Context &context, uint16_t stream_id, Account &account, Shared &shared, Request &request)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, account.name)}, connection_{create_connection(*this, shared.settings, context)},
      decode_buffer_{shared.settings.misc.decode_buffer_size, MAX_DECODE_BUFFER_DEPTH},
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .listen_key = create_metrics(shared.settings, name_, "listen_key"sv),
          .listen_key_ack = create_metrics(shared.settings, name_, "listen_key_ack"sv),
          .account = create_metrics(shared.settings, name_, "account"sv),
          .account_ack = create_metrics(shared.settings, name_, "account_ack"sv),
          .open_orders = create_metrics(shared.settings, name_, "open_orders"sv),
          .open_orders_ack = create_metrics(shared.settings, name_, "open_orders_ack"sv),
          .trades = create_metrics(shared.settings, name_, "trades"sv),
          .trades_ack = create_metrics(shared.settings, name_, "trades_ack"sv),
          .new_order = create_metrics(shared.settings, name_, "new_order"sv),
          .new_order_ack = create_metrics(shared.settings, name_, "new_order_ack"sv),
          .cancel_order = create_metrics(shared.settings, name_, "cancel_order"sv),
          .cancel_order_ack = create_metrics(shared.settings, name_, "cancel_order_ack"sv),
          .cancel_all_open_orders = create_metrics(shared.settings, name_, "cancel_all_open_orders"sv),
          .cancel_all_open_orders_ack = create_metrics(shared.settings, name_, "cancel_all_open_orders_ack"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
      },
      rate_limiter_{
          .requests_1m = create_metrics(shared.settings, name_, "requests"sv, "1m"sv),
      },
      account_{account}, shared_{shared}, request_{request}, download_{shared.settings.rest.request_timeout, [this](auto state) { return download(state); }} {
}

void OrderEntryMargin::operator()(Event<Start> const &) {
  (*connection_).start();
}

void OrderEntryMargin::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void OrderEntryMargin::operator()(Event<Timer> const &event) {
  auto &[message_info, timer] = event;
  (*connection_).refresh(timer.now);
  refresh_listen_key(timer.now);
  // XXX HANS only master
  if (ready() && !downloading()) {
    // spot
    if (!downloading() && request_.respond_account < request_.request_account) {
      get_account({});
      download_account_ = true;
    }
    if (!downloading() && request_.respond_orders < request_.request_orders) {
      get_open_orders({});
      download_orders_ = true;
    }
    if (!downloading() && request_.respond_trades < request_.request_trades) {
      get_trades({});
      download_trades_ = true;
    }
    // margin cross
    if (!downloading() && request_.respond_account_cross < request_.request_account_cross) {
      get_account(MarginMode::CROSS);
      download_account_cross_ = true;
    }
    if (!downloading() && request_.respond_orders_cross < request_.request_orders_cross) {
      get_open_orders(MarginMode::CROSS);
      download_orders_cross_ = true;
    }
    if (!downloading() && request_.respond_trades_cross < request_.request_trades_cross) {
      get_trades(MarginMode::CROSS);
      download_trades_cross_ = true;
    }
    // timer
    if (!downloading() && shared_.settings.rest.download_borrowed_freq.count() && (next_poll_borrowed_ < timer.now || next_poll_borrowed_.count() == 0)) {
      get_account_cross_on_timer();
      download_account_cross_on_timer_ = true;
      next_poll_borrowed_ = timer.now + shared_.settings.rest.download_borrowed_freq;
    }
  }
}

void OrderEntryMargin::operator()(metrics::Writer &writer) const {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.listen_key, metrics::Type::PROFILE)
      .write(profile_.listen_key_ack, metrics::Type::PROFILE)
      .write(profile_.account, metrics::Type::PROFILE)
      .write(profile_.account_ack, metrics::Type::PROFILE)
      .write(profile_.open_orders, metrics::Type::PROFILE)
      .write(profile_.open_orders_ack, metrics::Type::PROFILE)
      .write(profile_.trades, metrics::Type::PROFILE)
      .write(profile_.trades_ack, metrics::Type::PROFILE)
      .write(profile_.new_order, metrics::Type::PROFILE)
      .write(profile_.new_order_ack, metrics::Type::PROFILE)
      .write(profile_.cancel_order, metrics::Type::PROFILE)
      .write(profile_.cancel_order_ack, metrics::Type::PROFILE)
      .write(profile_.cancel_all_open_orders, metrics::Type::PROFILE)
      .write(profile_.cancel_all_open_orders_ack, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      // rate limiter
      .write(rate_limiter_.requests_1m, metrics::Type::RATE_LIMITER);
}

uint16_t OrderEntryMargin::operator()(Event<CreateOrder> const &event, server::oms::Order const &order, std::string_view const &request_id) {
  new_order(event, order, request_id);
  return stream_id_;
}

uint16_t OrderEntryMargin::operator()(
    Event<ModifyOrder> const &,
    server::oms::Order const &,
    [[maybe_unused]] std::string_view const &request_id,
    [[maybe_unused]] std::string_view const &previous_request_id) {
  throw server::oms::NotSupported{"not supported"sv};
  return stream_id_;
}

uint16_t OrderEntryMargin::operator()(
    Event<CancelOrder> const &event, server::oms::Order const &order, std::string_view const &request_id, std::string_view const &previous_request_id) {
  (*this).cancel_order(event, order, request_id, previous_request_id);
  return stream_id_;
}

uint16_t OrderEntryMargin::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  cancel_all_open_orders(event, request_id);
  return stream_id_;
}

void OrderEntryMargin::operator()(Trace<web::rest::Client::Connected> const &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void OrderEntryMargin::operator()(Trace<web::rest::Client::Disconnected> const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading()) {
    download_.reset();
  }
  download_account_ = false;
  download_orders_ = false;
  download_trades_ = false;
  download_account_cross_ = false;
  download_orders_cross_ = false;
  download_trades_cross_ = false;
  download_account_cross_on_timer_ = false;
}

void OrderEntryMargin::operator()(Trace<web::rest::Client::Header> const &event) {
  auto &[trace_info, header] = event;
  if (utils::case_insensitive_compare(header.name, "x-mbx-used-weight-1m"sv) == 0) {
    try {
      auto value = utils::charconv::from_string_relaxed<int64_t>(header.value);
      rate_limiter_.requests_1m.set(value);
    } catch (RuntimeError &) {
      log::warn<5>(R"(Failed to parse text="{}")"sv, header.value);
    }
  }
}

void OrderEntryMargin::operator()(Trace<web::rest::Client::Latency> const &event) {
  auto &[trace_info, latency] = event;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = account_.name,
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntryMargin::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    auto stream_status = StreamStatus{
        .stream_id = stream_id_,
        .account = account_.name,
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::HTTP,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
        .interface = (*connection_).get_interface(),
        .authority = (*connection_).get_current_authority(),
        .path = (*connection_).get_current_path(),
        .proxy = (*connection_).get_proxy(),
    };
    log::info("stream_status={}"sv, stream_status);
    create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t OrderEntryMargin::download(OrderEntryState state) {
  switch (state) {
    using enum OrderEntryState;
    case UNDEFINED:
      assert(false);
      break;
    case LISTEN_KEY:
      get_listen_key(MarginMode::CROSS);  // note! isolated requires the symbol... don't know how to
      return 1;
    case DONE:
      (*this)(ConnectionStatus::READY);
      assert(!ready_);
      ready_ = true;
      return 0;
  }
  assert(false);
  return 0;
}

// listen-key

void OrderEntryMargin::get_listen_key(MarginMode margin_mode) {
  profile_.listen_key([&]() {
    auto path = [&]() {
      switch (margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          break;
        case ISOLATED:
          return shared_.api.sapi.isolated_margin_user_data_stream;
        case CROSS:
          return shared_.api.sapi.margin_user_data_stream;
        case PORTFOLIO:
          break;
      }
      log::fatal("Unexpected"sv);
    }();
    log::warn("DEBUG margin_mode={}, path={}"sv, margin_mode, path);
    auto headers = account_.get_rest_headers();
    auto request = web::rest::Request{
        .method = web::http::Method::POST,
        .path = path,
        .query = {},
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this, margin_mode = margin_mode]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_listen_key_ack(event, margin_mode);
    };
    (*connection_)("listen_key"sv, request, callback);
  });
}

void OrderEntryMargin::get_listen_key_ack(Trace<web::rest::Response> const &event, MarginMode margin_mode) {
  auto const STATE = OrderEntryState::LISTEN_KEY;
  profile_.listen_key_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(account="{}", origin={}, error={}, status={}, text="{}")"sv, account_.name, origin, error, status, text);
      if (download_.downloading()) {
        download_.retry(STATE);
      }
    };
    auto handle_success = [&](auto &body) {
      json::ListenKeyAck listen_key{body};
      Trace event_2{event, listen_key};
      (*this)(event_2, margin_mode);
      switch (margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          // get_listen_key(MarginMode::ISOLATED);  // note! for isolated we also need the symbol...
          get_listen_key(MarginMode::CROSS);  // note! workaround
          break;
        case ISOLATED:
          get_listen_key(MarginMode::CROSS);
          break;
        case CROSS:
          download_.check_relaxed(STATE);  // note! done
          break;
        case PORTFOLIO:
          log::fatal("Unexpected"sv);
      }
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryMargin::operator()(Trace<json::ListenKeyAck> const &event, MarginMode margin_mode) {
  auto &[trace_info, listen_key_ack] = event;
  log::info<2>("listen_key_ack={}"sv, listen_key_ack);
  auto dispatch = [&](auto initial) {
    if (initial) {
      log::warn(R"(DEBUG Listen key has been acquired (margin_mode={}, value="{}"))"sv, margin_mode, listen_key_ack.listen_key);
      log::info<1>(R"(Listen key has been acquired (margin_mode={}, value="{}"))"sv, margin_mode, listen_key_ack.listen_key);
      auto listen_key_update = ListenKeyUpdate{
          .account = account_.name,
          .margin_mode = margin_mode,
          .listen_key = listen_key_ack.listen_key,
      };
      create_trace_and_dispatch(handler_, trace_info, listen_key_update);
    } else {
      log::info<1>("Listen key has been refreshed! (margin_mode={})"sv, margin_mode);
    }
  };
  auto update_spot = [&]() {
    bool initial = std::empty(listen_key_);
    if (utils::update(listen_key_, listen_key_ack.listen_key)) {
      dispatch(initial);
    }
    auto now = clock::get_system();
    listen_key_refresh_ = now + shared_.settings.rest.listen_key_refresh;
  };
  auto update_margin_cross = [&]() {
    bool initial = std::empty(listen_key_cross_);
    if (utils::update(listen_key_cross_, listen_key_ack.listen_key)) {
      dispatch(initial);
    }
    auto now = clock::get_system();
    listen_key_refresh_cross_ = now + shared_.settings.rest.listen_key_refresh;
  };
  switch (margin_mode) {
    using enum MarginMode;
    case UNDEFINED:
      update_spot();
      break;
    case ISOLATED:
      log::fatal("Unexpected"sv);  // note! not implemented
      break;
    case CROSS:
      update_margin_cross();
      break;
    case PORTFOLIO:
      log::fatal("Unexpected"sv);
  }
}

// account

void OrderEntryMargin::get_account(MarginMode margin_mode) {
  profile_.account([&]() {
    log::info("Download account... (margin_mode={})"sv, margin_mode);
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto path = [&]() {
      switch (margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          log::fatal("Unexpected"sv);  // check return shared_.api.sapi.account;
          break;
        case ISOLATED:
          log::fatal("Unexpected"sv);  // note! not implemented
          break;
        case CROSS:
          return shared_.api.sapi.cross_account;
        case PORTFOLIO:
          break;
      }
      log::fatal("Unexpected"sv);
    }();
    auto query = account_.create_rest_signature(now);
    auto headers = account_.get_rest_headers();
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = path,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this, margin_mode = margin_mode]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_account_ack(event, margin_mode);
    };
    (*connection_)("account"sv, request, callback);
  });
}

void OrderEntryMargin::get_account_ack(Trace<web::rest::Response> const &event, MarginMode margin_mode) {
  profile_.account_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(account="{}", origin={}, error={}, status={}, text="{}")"sv, account_.name, origin, error, status, text);
      switch (margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          request_.respond_account = clock::get_system();  // completion
          download_account_ = false;
          break;
        case ISOLATED:
          log::fatal("Unexpected"sv);  // note! not implemented
          break;
        case CROSS:
          request_.respond_account_cross = clock::get_system();  // completion
          download_account_cross_ = false;
          break;
        case PORTFOLIO:
          log::fatal("Unexpected"sv);
      }
    };
    auto handle_success = [&](auto &body) {
      log::warn(R"(DEBUG body="{}")"sv, body);
      switch (margin_mode) {
        using enum MarginMode;
        case UNDEFINED: {
          json::AccountAck account_ack{body, decode_buffer_};
          Trace event_2{event, account_ack};
          (*this)(event_2, margin_mode);
          request_.respond_account = clock::get_system();  // completion
          download_account_ = false;
          break;
        }
        case ISOLATED:
          log::fatal("Unexpected"sv);  // note! not implemented
        case CROSS: {
          json::CrossMarginAccount account{body, decode_buffer_};
          Trace event_2{event, account};
          (*this)(event_2, margin_mode);
          request_.respond_account_cross = clock::get_system();  // completion
          download_account_cross_ = false;
          break;
        }
        case PORTFOLIO:
          log::fatal("Unexpected"sv);
      }
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryMargin::operator()(Trace<json::AccountAck> const &event, MarginMode margin_mode) {
  auto &[trace_info, account_ack] = event;
  log::info<2>("account_ack={}"sv, account_ack);
  for (auto &item : account_ack.balances) {
    auto funds_update = FundsUpdate{
        .stream_id = stream_id_,
        .account = account_.name,
        .currency = item.asset,
        .margin_mode = margin_mode,
        .balance = item.free,
        .hold = item.locked,
        .borrowed = NaN,
        .external_account = {},
        .update_type = UpdateType::SNAPSHOT,
        .exchange_time_utc = account_ack.update_time,
        .sending_time_utc = account_ack.update_time,
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
}

void OrderEntryMargin::operator()(Trace<json::CrossMarginAccount> const &event, MarginMode margin_mode) {
  auto &[trace_info, account] = event;
  log::info<2>("account={}"sv, account);
  for (auto &item : account.user_assets) {
    auto funds_update = FundsUpdate{
        .stream_id = stream_id_,
        .account = account_.name,
        .currency = item.asset,
        .margin_mode = margin_mode,
        .balance = item.free,
        .hold = item.locked,
        .borrowed = item.borrowed,
        .external_account = {},
        .update_type = UpdateType::SNAPSHOT,
        .exchange_time_utc = {},
        .sending_time_utc = {},
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
}

// orders

// XXX FIXME TODO for margin => isIsolated true/false
void OrderEntryMargin::get_open_orders(MarginMode margin_mode) {
  profile_.open_orders([&]() {
    log::info("Download open orders... (margin_mode={})"sv, margin_mode);
    auto path = [&]() {
      switch (margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          break;
        case ISOLATED:
          break;  // note! not implemented
        case CROSS:
          return shared_.api.sapi.margin_open_orders;
        case PORTFOLIO:
          break;
      };
      log::fatal("Unexpected"sv);
    }();
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto query = account_.create_rest_signature(now);
    auto headers = account_.get_rest_headers();
    auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
    encode_buffer_.clear();
    fmt::format_to(
        std::back_inserter(encode_buffer_),
        R"({{)"
        R"("timestamp":{})"
        R"(}})"sv,
        timestamp.count());
    std::string body{std::data(encode_buffer_), std::size(encode_buffer_)};
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = path,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this, margin_mode = margin_mode]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_open_orders_ack(event, margin_mode);
    };
    (*connection_)("open_orders"sv, request, callback);
  });
}

void OrderEntryMargin::get_open_orders_ack(Trace<web::rest::Response> const &event, MarginMode margin_mode) {
  profile_.open_orders_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(account="{}", origin={}, error={}, status={}, text="{}")"sv, account_.name, origin, error, status, text);
      switch (margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          request_.respond_orders = clock::get_system();  // completion
          download_orders_ = false;
          break;
        case ISOLATED:
          log::fatal("Unexpected"sv);  // note! not implemented
        case CROSS:
          request_.respond_orders_cross = clock::get_system();  // completion
          download_orders_cross_ = false;
          break;
        case PORTFOLIO:
          log::fatal("Unexpected"sv);
      }
    };
    auto handle_success = [&](auto &body) {
      log::debug("body={}"sv, body);
      json::OpenOrdersAck open_orders_ack{body, decode_buffer_};
      Trace event_2{event, open_orders_ack};
      (*this)(event_2, margin_mode);
      switch (margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          request_.respond_orders = clock::get_system();  // completion
          download_orders_ = false;
          break;
        case ISOLATED:
          log::fatal("Unexpected"sv);  // note! not implemented
        case CROSS:
          request_.respond_orders_cross = clock::get_system();  // completion
          download_orders_cross_ = false;
          break;
        case PORTFOLIO:
          log::fatal("Unexpected"sv);
      }
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryMargin::operator()(Trace<json::OpenOrdersAck> const &event, MarginMode margin_mode) {
  auto &[trace_info, open_orders_ack] = event;
  for (auto &order : open_orders_ack.data) {
    log::info<2>("order={}"sv, order);
    if (std::empty(order.client_order_id)) {
      continue;
    }
    open_orders_symbols_.emplace(order.symbol);
    // XXX FIXME TODO validate order.is_isolated for margin_mode isolated + cross
    auto external_order_id = fmt::format("{}"sv, order.order_id);  // alloc
    auto order_update = server::oms::OrderUpdate{
        .account = account_.name,
        .exchange = shared_.settings.exchange,
        .symbol = order.symbol,
        .side = map(order.side),
        .position_effect = {},
        .margin_mode = margin_mode,
        .max_show_quantity = NaN,
        .order_type = map(order.type),
        .time_in_force = map(order.time_in_force),
        .execution_instructions = {},
        .create_time_utc = order.time,
        .update_time_utc = order.update_time,
        .external_account = {},
        .external_order_id = external_order_id,
        .client_order_id = {},
        .order_status = map(order.status),
        .quantity = order.orig_qty,
        .price = order.price,
        .stop_price = order.stop_price,
        .leverage = NaN,
        .remaining_quantity = NaN,
        .traded_quantity = order.executed_qty,
        .average_traded_price = {},
        .last_traded_quantity = {},
        .last_traded_price = {},
        .last_liquidity = {},
        .routing_id = {},
        .max_request_version = {},
        .max_response_version = {},
        .max_accepted_version = {},
        .update_type = UpdateType::SNAPSHOT,
        .sending_time_utc = {},
    };
    Trace event_2{trace_info, order_update};
    (*this)(event_2, order.client_order_id);
  }
}

// trades

// XXX FIXME TODO download margin => isIsolated true/false

void OrderEntryMargin::get_trades(MarginMode margin_mode) {
  profile_.trades([&]() {
    log::info("Download trades... (margin_mode={})"sv, margin_mode);
    auto &symbols = shared_.settings.download.symbols;
    for (auto &symbol : symbols) {
      auto now = clock::get_realtime<std::chrono::milliseconds>();
      auto lookback = get_download_trades_lookback(shared_.settings, download_trades_is_first_);
      auto limit = shared_.settings.download.trades_limit ? shared_.settings.download.trades_limit : DOWNLOAD_TRADES_LIMIT;
      log::info<1>("Download trades: lookback={}"sv, lookback);
      auto headers = account_.get_rest_headers();
      auto body = json::Encoder::my_trades_url(encode_buffer_, symbol, lookback, limit, now);
      auto query = account_.create_rest_signature_body(now, body);
      auto request = web::rest::Request{
          .method = web::http::Method::GET,
          .path = shared_.api.sapi.margin_my_trades,
          .query = query,
          .accept = web::http::Accept::APPLICATION_JSON,
          .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
          .headers = headers,
          .body = body,
          .quality_of_service = {},
      };
      auto callback = [this, margin_mode = margin_mode]([[maybe_unused]] auto &request_id, auto &response) {
        TraceInfo trace_info;
        Trace event{trace_info, response};
        get_trades_ack(event, margin_mode);
      };
      (*connection_)("my-trades"sv, request, callback);
    }
  });
}

void OrderEntryMargin::get_trades_ack(Trace<web::rest::Response> const &event, MarginMode margin_mode) {
  profile_.trades_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(account="{}", origin={}, error={}, status={}, text="{}")"sv, account_.name, origin, error, status, text);
      switch (margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          request_.respond_trades = clock::get_system();  // completion
          download_trades_ = false;
          break;
        case ISOLATED:
          log::fatal("Unexpected"sv);  // note! not implemented
        case CROSS:
          request_.respond_trades_cross = clock::get_system();  // completion
          download_trades_cross_ = false;
          break;
        case PORTFOLIO:
          log::fatal("Unexpected"sv);
      }
    };
    auto handle_success = [&](auto &body) {
      json::TradesAck trades_ack{body, decode_buffer_};
      Trace event_2{event, trades_ack};
      (*this)(event_2, margin_mode);
      switch (margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          request_.respond_trades = clock::get_system();  // completion
          download_trades_ = false;
          download_trades_is_first_ = false;
          break;
        case ISOLATED:
          log::fatal("Unexpected"sv);  // note! not implemented
        case CROSS:
          request_.respond_trades_cross = clock::get_system();  // completion
          download_trades_cross_ = false;
          download_trades_cross_is_first_ = false;
          break;
        case PORTFOLIO:
          log::fatal("Unexpected"sv);
      }
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryMargin::operator()(Trace<json::TradesAck> const &event, MarginMode) {
  auto &[trace_info, trades_ack] = event;
  for (auto &trade : trades_ack.data) {
    log::info<2>("trade={}"sv, trade);
    /*
    if (std::empty(order.client_order_id))
      continue;
    open_orders_symbols_.emplace(order.symbol);
    auto side = json::map(order.side);
    auto order_type = json::map(order.type);
    auto time_in_force = json::map(order.time_in_force);
    auto external_order_id = fmt::format("{}"sv, order.order_id);  // alloc
    auto order_status = json::map(order.status);
    auto order_update = server::oms::OrderUpdate{
        .account = account_.name,
        .exchange = shared_.settings.exchange,
        .symbol = order.symbol,
        .side = side,
        .position_effect = {},
        .margin_mode = margin_mode,
        .max_show_quantity = NaN,
        .order_type = order_type,
        .time_in_force = time_in_force,
        .execution_instructions = {},
        .create_time_utc = order.time,
        .update_time_utc = order.update_time,
        .external_account = {},
        .external_order_id = external_order_id,
        .client_order_id = {},
        .order_status = order_status,
        .quantity = order.orig_qty,
        .price = order.price,
        .stop_price = order.stop_price,
        .leverage = NaN,
        .remaining_quantity = NaN,
        .traded_quantity = order.executed_qty,
        .average_traded_price = {},
        .last_traded_quantity = {},
        .last_traded_price = {},
        .last_liquidity = {},
        .routing_id = {},
        .max_request_version = {},
        .max_response_version = {},
        .max_accepted_version = {},
        .update_type = UpdateType::SNAPSHOT,
        .sending_time_utc = {},
    };
    Trace event_2{trace_info, order_update};
    (*this)(event_2, order.client_order_id);
    */
  }
}

// timer

void OrderEntryMargin::get_account_cross_on_timer() {
  profile_.account([&]() {
    log::warn("DEBUG Download borrowed amount ON TIMER..."sv);
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto path = shared_.api.sapi.cross_account;
    auto query = account_.create_rest_signature(now);
    auto headers = account_.get_rest_headers();
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = path,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_account_cross_on_timer_ack(event);
    };
    (*connection_)("account"sv, request, callback);
  });
}

void OrderEntryMargin::get_account_cross_on_timer_ack(Trace<web::rest::Response> const &event) {
  profile_.account_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(account="{}", origin={}, error={}, status={}, text="{}")"sv, account_.name, origin, error, status, text);
      request_.respond_account_cross = clock::get_system();  // completion
      download_account_cross_on_timer_ = false;
    };
    auto handle_success = [&](auto &body) {
      log::warn(R"(DEBUG body="{}")"sv, body);
      json::CrossMarginAccount account{body, decode_buffer_};
      Trace event_2{event, account};
      (*this)(event_2);
      request_.respond_account_cross = clock::get_system();  // completion
      download_account_cross_on_timer_ = false;
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryMargin::operator()(Trace<json::CrossMarginAccount> const &event) {
  auto &[trace_info, account] = event;
  log::info<2>("account={}"sv, account);
  for (auto &item : account.user_assets) {
    auto funds_update = FundsUpdate{
        .stream_id = stream_id_,
        .account = account_.name,
        .currency = item.asset,
        .margin_mode = MarginMode::CROSS,
        .balance = NaN,             // note!
        .hold = NaN,                // note!
        .borrowed = item.borrowed,  // note!
        .external_account = {},
        .update_type = UpdateType::INCREMENTAL,  // note! avoid replacing other fields
        .exchange_time_utc = {},
        .sending_time_utc = {},
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
}

// ...

void OrderEntryMargin::refresh_listen_key(std::chrono::nanoseconds now) {
  if (!ready_) {
    return;
  }
  if (listen_key_refresh_.count() != 0 && listen_key_refresh_ < now) {
    log::info<1>("Refreshing listen key..."sv);
    listen_key_refresh_ = now + shared_.settings.rest.listen_key_refresh;
    get_listen_key(MarginMode::UNDEFINED);
  }
  if (listen_key_refresh_cross_.count() != 0 && listen_key_refresh_cross_ < now) {
    log::info<1>("Refreshing listen key..."sv);
    listen_key_refresh_cross_ = now + shared_.settings.rest.listen_key_refresh;
    get_listen_key(MarginMode::CROSS);
  }
}

// new-order

void OrderEntryMargin::new_order(Event<CreateOrder> const &event, server::oms::Order const &order, std::string_view const &request_id) {
  profile_.new_order([&]() {
    if (!ready()) {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, create_order] = event;
    open_orders_symbols_.emplace(create_order.symbol);
    auto &create_order_template = shared_.get_create_order_template(create_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto body =
        json::Encoder::new_order_url(encode_buffer_, create_order, order, request_id, create_order_template, recv_window, shared_.api.margin_side_effect_type);
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto query = account_.create_rest_signature_body(now, body);
    auto headers = account_.get_rest_headers();
    auto path = shared_.api.sapi.margin_order;
    auto request = web::rest::Request{
        .method = web::http::Method::POST,
        .path = path,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
        .headers = headers,
        .body = body,
        .quality_of_service = io::QualityOfService::IMMEDIATE,
    };
    log::warn("DEBUG request={}"sv, request);
    auto callback = [this, user_id = message_info.source, order_id = create_order.order_id]([[maybe_unused]] auto &request_id, auto &response) {
      uint32_t version = 1;
      TraceInfo trace_info;
      Trace event{trace_info, response};
      new_order_ack(event, user_id, order_id, version);
    };
    (*connection_)(request_id, request, callback);
  });
}

void OrderEntryMargin::new_order_ack(Trace<web::rest::Response> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  profile_.new_order_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      auto response = server::oms::Response{
          .request_type = RequestType::CREATE_ORDER,
          .origin = origin,
          .request_status = status,
          .error = error,
          .text = text,
          .version = version,
          .request_id = {},
          .external_order_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      Trace event_2{event, response};
      (*this)(event_2, user_id, order_id);
    };
    auto handle_success = [&](auto &body) {
      log::warn(R"(DEBUG body="{}")"sv, body);
      json::NewOrderAck new_order_ack{body, decode_buffer_};
      Trace event_2{event, new_order_ack};
      (*this)(event_2, user_id, order_id, version);
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryMargin::operator()(Trace<json::NewOrderAck> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  auto &[trace_info, new_order_ack] = event;
  log::info<2>("new_order_ack={}, user_id={}, order_id={}, version={}"sv, new_order_ack, user_id, order_id, version);
  auto margin_mode = new_order_ack.is_isolated ? MarginMode::ISOLATED : MarginMode::CROSS;
  auto external_order_id = fmt::format("{}"sv, new_order_ack.order_id);  // alloc
  auto order_status = map(new_order_ack.status).template get<OrderStatus>();
  // LIMIT_MAKER orders do not return any order state + we only end up here if we receive HTTP status OK
  if (order_status == OrderStatus{}) {
    order_status = OrderStatus::WORKING;
  }
  auto remaining_quantity = new_order_ack.orig_qty - new_order_ack.executed_qty;
  auto average_traded_price = utils::is_zero(new_order_ack.executed_qty) ? NaN : (new_order_ack.cummulative_quote_qty / new_order_ack.executed_qty);
  auto last_traded_quantity = 0.0;  // note! could also use new_order_ack.executed_qty
  auto tmp = 0.0;
  for (auto &item : new_order_ack.fills) {
    last_traded_quantity += item.qty;
    tmp += item.price * item.qty;
  }
  auto last_traded_price = NaN;  // note! could also use average_traded_price
  if (utils::is_greater(last_traded_quantity, 0.0)) {
    last_traded_price = tmp / last_traded_quantity;
  }
  auto response = server::oms::Response{
      .request_type = RequestType::CREATE_ORDER,
      .origin = Origin::EXCHANGE,
      .request_status = RequestStatus::ACCEPTED,
      .error = {},
      .text = {},
      .version = version,
      .request_id = {},
      .external_order_id = {},
      .quantity = NaN,
      .price = NaN,
  };
  auto order_update = server::oms::OrderUpdate{
      .account = account_.name,
      .exchange = shared_.settings.exchange,
      .symbol = new_order_ack.symbol,
      .side = map(new_order_ack.side),
      .position_effect = {},
      .margin_mode = margin_mode,
      .max_show_quantity = NaN,
      .order_type = map(new_order_ack.type),
      .time_in_force = map(new_order_ack.time_in_force),
      .execution_instructions = {},
      .create_time_utc = {},
      .update_time_utc = new_order_ack.transact_time,
      .external_account = {},
      .external_order_id = external_order_id,
      .client_order_id = {},
      .order_status = order_status,
      .quantity = new_order_ack.orig_qty,
      .price = new_order_ack.price,
      .stop_price = NaN,
      .leverage = NaN,
      .remaining_quantity = remaining_quantity,
      .traded_quantity = new_order_ack.executed_qty,
      .average_traded_price = average_traded_price,
      .last_traded_quantity = last_traded_quantity,
      .last_traded_price = last_traded_price,
      .last_liquidity = {},
      .routing_id = {},
      .max_request_version = {},
      .max_response_version = {},
      .max_accepted_version = {},
      .update_type = UpdateType::INCREMENTAL,
      .sending_time_utc = {},
  };
  Trace event_2{trace_info, response};
  (*this)(event_2, user_id, order_id, order_update);
}

// cancel-order

void OrderEntryMargin::cancel_order(
    Event<CancelOrder> const &event, server::oms::Order const &order, std::string_view const &request_id, std::string_view const &previous_request_id) {
  profile_.cancel_order([&]() {
    if (!ready()) {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, cancel_order] = event;
    auto &cancel_order_template = shared_.get_cancel_order_template(cancel_order.request_template);
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    auto body = json::Encoder::cancel_order_url(encode_buffer_, cancel_order, order, request_id, previous_request_id, cancel_order_template, recv_window);
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto query = account_.create_rest_signature_body(now, body);
    auto headers = account_.get_rest_headers();
    auto path = [&]() -> std::string_view {
      switch (order.margin_mode) {
        using enum MarginMode;
        case UNDEFINED:
          return shared_.api.sapi.margin_order;
        case ISOLATED:
        case CROSS:
          return shared_.api.sapi.margin_order;
        case PORTFOLIO:
          throw server::oms::Rejected{Origin::GATEWAY, Error::INVALID_MARGIN_MODE, "internal error"sv};
      };
      log::fatal("Unexpected"sv);
    }();
    auto request = web::rest::Request{
        .method = web::http::Method::DELETE,
        .path = path,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
        .headers = headers,
        .body = body,
        .quality_of_service = io::QualityOfService::IMMEDIATE,
    };
    auto callback = [this, user_id = message_info.source, order_id = cancel_order.order_id, version = cancel_order.version](
                        [[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      cancel_order_ack(event, user_id, order_id, version);
    };
    (*connection_)(request_id, request, callback);
  });
}

void OrderEntryMargin::cancel_order_ack(Trace<web::rest::Response> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  profile_.cancel_order_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      auto response = server::oms::Response{
          .request_type = RequestType::CANCEL_ORDER,
          .origin = origin,
          .request_status = status,
          .error = error,
          .text = text,
          .version = version,
          .request_id = {},
          .external_order_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      Trace event_2{event, response};
      (*this)(event_2, user_id, order_id);
    };
    auto handle_success = [&](auto &body) {
      log::warn(R"(DEBUG body="{}")"sv, body);
      json::CancelOrderAck cancel_order_ack{body};
      Trace event_2{event, cancel_order_ack};
      (*this)(event_2, user_id, order_id, version);
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryMargin::operator()(Trace<json::CancelOrderAck> const &event, uint8_t user_id, uint64_t order_id, uint32_t version) {
  auto &[trace_info, cancel_order_ack] = event;
  log::info<2>("cancel_order_ack={}, user_id={}, order_id={}, version={}"sv, cancel_order_ack, user_id, order_id, version);
  auto margin_mode = cancel_order_ack.is_isolated ? MarginMode::ISOLATED : MarginMode::CROSS;
  auto external_order_id = fmt::format("{}"sv, cancel_order_ack.order_id);  // alloc
  auto response = server::oms::Response{
      .request_type = RequestType::CANCEL_ORDER,
      .origin = Origin::EXCHANGE,
      .request_status = RequestStatus::ACCEPTED,
      .error = {},
      .text = {},
      .version = version,
      .request_id = {},
      .external_order_id = {},
      .quantity = NaN,
      .price = NaN,
  };
  auto order_update = server::oms::OrderUpdate{
      .account = account_.name,
      .exchange = shared_.settings.exchange,
      .symbol = cancel_order_ack.symbol,
      .side = map(cancel_order_ack.side),
      .position_effect = {},
      .margin_mode = margin_mode,
      .max_show_quantity = NaN,
      .order_type = map(cancel_order_ack.type),
      .time_in_force = map(cancel_order_ack.time_in_force),
      .execution_instructions = {},
      .create_time_utc = {},
      .update_time_utc = {},
      .external_account = {},
      .external_order_id = external_order_id,
      .client_order_id = {},
      .order_status = map(cancel_order_ack.status),
      .quantity = cancel_order_ack.orig_qty,
      .price = cancel_order_ack.price,
      .stop_price = NaN,
      .leverage = NaN,
      .remaining_quantity = NaN,
      .traded_quantity = cancel_order_ack.executed_qty,
      .average_traded_price = NaN,
      .last_traded_quantity = NaN,
      .last_traded_price = NaN,
      .last_liquidity = {},
      .routing_id = {},
      .max_request_version = {},
      .max_response_version = {},
      .max_accepted_version = {},
      .update_type = UpdateType::INCREMENTAL,
      .sending_time_utc = {},
  };
  Trace event_2{trace_info, response};
  (*this)(event_2, user_id, order_id, order_update);
}

void OrderEntryMargin::cancel_all_open_orders(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  profile_.cancel_all_open_orders([&]() {
    if (!ready()) [[unlikely]] {
      throw server::oms::NotReady{"not ready"sv};
    }
    auto &[message_info, cancel_all_orders] = event;
    auto send_ack = [&](auto &symbol) {
      auto cancel_all_orders_ack = CancelAllOrdersAck{
          .stream_id = stream_id_,
          .account = account_.name,
          .order_id = cancel_all_orders.order_id,
          .exchange = cancel_all_orders.exchange,
          .symbol = symbol,
          .side = cancel_all_orders.side,
          .origin = Origin::GATEWAY,
          .request_status = RequestStatus::FORWARDED,
          .error = {},
          .text = {},
          .request_id = request_id,
          .external_account = {},
          .number_of_affected_orders = {},
          .round_trip_latency = {},
          .user = {},
          .strategy_id = cancel_all_orders.strategy_id,
      };
      TraceInfo trace_info{message_info};
      Trace event_2{trace_info, cancel_all_orders_ack};
      shared_(event_2);
    };
    auto recv_window = std::chrono::duration_cast<std::chrono::milliseconds>(shared_.settings.rest.order_recv_window);
    for (auto &symbol : open_orders_symbols_) {
      if (!std::empty(cancel_all_orders.symbol) && symbol != cancel_all_orders.symbol) {
        continue;
      }
      auto helper = [&](auto margin_mode) {
        auto path = [&]() -> std::string_view {
          switch (margin_mode) {
            using enum MarginMode;
            case UNDEFINED:
              throw server::oms::Rejected{Origin::GATEWAY, Error::INVALID_MARGIN_MODE, "internal error"sv};
            case ISOLATED:
            case CROSS:
              return shared_.api.sapi.margin_open_orders;
            case PORTFOLIO:
              throw server::oms::Rejected{Origin::GATEWAY, Error::INVALID_MARGIN_MODE, "internal error"sv};
          };
          log::fatal("Unexpected"sv);
        }();
        auto body = json::Encoder::cancel_all_open_orders_url(encode_buffer_, symbol, margin_mode, recv_window);
        auto now = clock::get_realtime<std::chrono::milliseconds>();
        auto query = account_.create_rest_signature_body(now, body);
        auto headers = account_.get_rest_headers();
        auto request = web::rest::Request{
            .method = web::http::Method::DELETE,
            .path = path,
            .query = query,
            .accept = web::http::Accept::APPLICATION_JSON,
            .content_type = web::http::ContentType::APPLICATION_X_WWW_FORM_URLENCODED,
            .headers = headers,
            .body = body,
            .quality_of_service = io::QualityOfService::IMMEDIATE,
        };
        auto callback = [&]([[maybe_unused]] auto &request_id, auto &response) {
          TraceInfo trace_info{event};
          Trace event{trace_info, response};
          cancel_all_open_orders_ack(event, request_id);
        };
        (*connection_)(request_id, request, callback);
      };
      helper(MarginMode::ISOLATED);
      helper(MarginMode::CROSS);
      send_ack(symbol);
    }
  });
}

void OrderEntryMargin::cancel_all_open_orders_ack(Trace<web::rest::Response> const &event, std::string_view const &request_id) {
  profile_.cancel_all_open_orders_ack([&]() {
    auto send_ack = [&](auto status, Error error, std::string_view const &text) {
      auto cancel_all_orders_ack = CancelAllOrdersAck{
          .stream_id = stream_id_,
          .account = account_.name,
          .order_id = {},
          .exchange = {},
          .symbol = {},
          .side = {},
          .origin = Origin::EXCHANGE,
          .request_status = status,
          .error = error,
          .text = text,
          .request_id = request_id,
          .external_account = {},
          .number_of_affected_orders = {},
          .round_trip_latency = {},
          .user = {},
          .strategy_id = {},
      };
      Trace event_2{event, cancel_all_orders_ack};
      shared_(event_2);
    };
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      switch (error) {
        using enum Error;
        case TOO_LATE_TO_MODIFY_OR_CANCEL:
          break;
        default:
          log::warn(R"(origin={}, error={}, status={}, text="{}")"sv, origin, error, status, text);
      }
      send_ack(RequestStatus::REJECTED, error, text);
    };
    auto handle_success = [&](auto &body) {
      log::warn(R"(DEBUG body="{}")"sv, body);
      json::CancelAllOpenOrdersAck cancel_all_open_orders_ack{body, decode_buffer_};
      Trace event_2{event, cancel_all_open_orders_ack};
      (*this)(event_2);
      send_ack(RequestStatus::ACCEPTED, {}, {});
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntryMargin::operator()(Trace<json::CancelAllOpenOrdersAck> const &event) {
  auto &[trace_info, cancel_all_open_orders_ack] = event;
  log::info<2>("cancel_all_open_orders_ack={}"sv, cancel_all_open_orders_ack);
  for (auto &order : cancel_all_open_orders_ack.data) {
    if (std::empty(order.client_order_id)) {
      continue;
    }
    auto external_order_id = fmt::format("{}"sv, order.order_id);  // alloc
    auto order_update = server::oms::OrderUpdate{
        .account = account_.name,
        .exchange = shared_.settings.exchange,
        .symbol = order.symbol,
        .side = map(order.side),
        .position_effect = {},
        .margin_mode = {},
        .max_show_quantity = NaN,
        .order_type = map(order.type),
        .time_in_force = map(order.time_in_force),
        .execution_instructions = {},
        .create_time_utc = order.time,
        .update_time_utc = order.update_time,
        .external_account = {},
        .external_order_id = external_order_id,
        .client_order_id = {},
        .order_status = map(order.status),
        .quantity = order.orig_qty,
        .price = order.price,
        .stop_price = order.stop_price,
        .leverage = NaN,
        .remaining_quantity = NaN,
        .traded_quantity = order.executed_qty,
        .average_traded_price = {},
        .last_traded_quantity = {},
        .last_traded_price = {},
        .last_liquidity = {},
        .routing_id = {},
        .max_request_version = {},
        .max_response_version = {},
        .max_accepted_version = {},
        .update_type = UpdateType::INCREMENTAL,
        .sending_time_utc = {},
    };
    shared_.update_order(order.client_order_id, stream_id_, trace_info, order_update, []([[maybe_unused]] auto &order) {});
  }
}

void OrderEntryMargin::process_response(web::rest::Response const &response, auto error_handler, auto success_handler) {
  try {
    auto [status, category, body] = response.result();
    switch (category) {
      using enum web::http::Category;
      case UNKNOWN:
      case INFORMATIONAL_RESPONSE:
        response.expect(web::http::Status::OK);  // throws
        break;
      case SUCCESS:
        success_handler(body);
        break;
      case REDIRECTION:
        log::fatal("Unexpected: URL is being redirected"sv);
      case CLIENT_ERROR:
        switch (status) {
          using enum web::http::Status;
          case FORBIDDEN:           // 403
            waf_limit_violation();  // note! this is *very* serious
            [[fallthrough]];
          case I_AM_A_TEAPOT:        // 418
          case TOO_MANY_REQUESTS: {  // 429
            auto message = fmt::format("{}"sv, status);
            error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::REQUEST_RATE_LIMIT_REACHED, message);
            break;
          }
          case CONFLICT:  // 409
            assert(false);
            [[fallthrough]];
          default: {
            json::Error error{body};
            error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, json::guess_error(error.code), error.msg);
          }
        }
        break;
      case SERVER_ERROR: {
        auto message = fmt::format("{}"sv, status);
        error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::UNKNOWN, message);
        break;
      }
    }
  } catch (server::oms::Exception &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(e.origin, e.status, e.error, e.what());
  } catch (NetworkError &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(Origin::GATEWAY, e.request_status(), e.error(), e.what());
  } catch (std::exception &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(Origin::EXCHANGE, RequestStatus::ERROR, Error::UNKNOWN, e.what());
  }
}

template <typename... Args>
void OrderEntryMargin::operator()(Trace<server::oms::Response> const &event, uint8_t user_id, uint64_t order_id, Args &&...args) {
  auto &[trace_info, response] = event;
  if (shared_.update_order(user_id, order_id, stream_id_, trace_info, response, std::forward<Args>(args)..., []([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
  }
}

void OrderEntryMargin::operator()(Trace<server::oms::OrderUpdate> const &event, std::string_view const &client_order_id) {
  auto &[trace_info, order_update] = event;
  if (shared_.update_order(client_order_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
  } else {
    log::warn("*** EXTERNAL ORDER ***"sv);
  }
}

// note! used by cancel-replace
template <typename Parse, typename Callback>
void OrderEntryMargin::dispatch_error_2(
    web::rest::Response const &response, web::http::Category category, web::http::Status status, Parse parse, Callback callback) {
  switch (category) {
    using enum web::http::Category;
    case UNKNOWN:
      break;
    case INFORMATIONAL_RESPONSE:
    case SUCCESS:
    case REDIRECTION:
      assert(false);
      break;
    case CLIENT_ERROR:
      try {
        // HTTP 4XX return codes are used for malformed requests; the issue is on the sender's side.
        // HTTP 403 return code is used when the WAF Limit (Web Application Firewall) has been violated.
        // HTTP 409 return code is used when a cancelReplace order partially succeeds. (e.g. if the
        //   cancellation of the order fails but the new order placement succeeds.)
        // HTTP 418 return code is used when an IP has been auto-banned for continuing to send requests
        //   after receiving 429 codes.
        // HTTP 429 return code is used when breaking a request rate limit.
        switch (status) {
          using enum web::http::Status;
          case FORBIDDEN:            // 403
          case I_AM_A_TEAPOT:        // 418
          case TOO_MANY_REQUESTS: {  // 429
            auto retry_after = get_retry_after(response);
            if (retry_after.count()) {
              (*connection_).suspend(retry_after);
            }
            auto text = fmt::format("{}"sv, status);
            callback(RequestStatus::REJECTED, Error::REQUEST_RATE_LIMIT_REACHED, text);
            break;
          }
          case CONFLICT:  // 409
            assert(false);
            [[fallthrough]];
          default:
            parse();
        }
      } catch (std::exception &e) {  // parse error
        callback(RequestStatus::ERROR, Error::UNKNOWN, e.what());
      }
      break;
    case SERVER_ERROR: {
      // HTTP 5XX return codes are used for internal errors; the issue is on Binance's side.
      //   It is important to NOT treat this as a failure operation; the execution status is UNKNOWN
      //   and could have been a success.
      auto text = fmt::format("{}"sv, status);
      callback(RequestStatus::REJECTED, Error::UNKNOWN, text);
      break;
    }
  }
}

void OrderEntryMargin::test(web::http::Status status) {
  if (status != web::http::Status::FORBIDDEN) [[likely]] {
    return;
  }
  waf_limit_violation();
}

void OrderEntryMargin::waf_limit_violation() {
  if (shared_.settings.rest.terminate_on_403) {
    log::fatal("WAF limit violation"sv);
  } else {
    log::warn("WAF limit violation"sv);
    (*connection_).suspend(shared_.settings.rest.back_off_delay);
  }
}

}  // namespace binance
}  // namespace roq
