/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/gateway.h"

#include <limits>
#include <utility>

#include "roq/logging.h"
#include "roq/format.h"

#include "roq/core/charconv.h"
#include "roq/core/clock.h"
#include "roq/core/utils.h"
#include "roq/core/view.h"

#include "roq/binance/options.h"

#include "roq/binance/json/utils.h"

namespace roq {
namespace binance {

constexpr auto DEFAULT_MULTIPLIER = double{1.0};

constexpr auto TOLERANCE = double{1.0e-10};

template <typename T>
static bool mbp_update(
    auto& data,
    size_t& offset,
    const T& item) {
  auto& obj = data[offset];
  new (&obj) MBPUpdate {
    .price = item.first,
    .quantity = item.second,
  };
  ++offset;
  return offset < data.size();
}

template <typename T>
static bool trade_update(
    auto& data,
    size_t& offset,
    const T& item) {
  auto& obj = data[offset];
  /*
  new (&obj) Trade {
    .side = json::map(item.side),  // XXX check
    .price = item.price,
    .quantity = item.size,
    .trade_id = {},
  };
  core::copy_to(
      item.trd_match_id,
      obj.trade_id);
      */
  ++offset;
  return offset < data.size();
}

Gateway::Gateway(
    server::Dispatcher& dispatcher,
    const Config& config)
    : _dispatcher(dispatcher),
      _account(config.get_account()),
      _random(
          config.get_api_key(),
          config.get_secret()),
      _dns_base(_base, true),
      _web_socket {
        .connection = {
          *this,
          config,
          _random,
          _base,
          _dns_base,
          _ssl_context,
        },
      },
      _rest {
        .connection = {
          *this,
          config,
          _random,
          _base,
          _dns_base,
          _ssl_context,
        },
        .download = RestDownload(
            std::chrono::seconds { FLAGS_download_timeout_secs },
            [this](auto state) {
              return download(state);
            }),
      },
      _bid(FLAGS_cache_mbp_max_depth),
      _ask(FLAGS_cache_mbp_max_depth),
      _trade(FLAGS_max_trades) {
  LOG_IF(WARNING, FLAGS_cancel_on_disconnect == false)(
      "Orders will *NOT* be cancelled on disconnect");
}

void Gateway::operator()(const StartEvent& event) {
  LOG(INFO)("Starting the gateway...");
  _web_socket.connection(event);
  _rest.connection(event);
}

void Gateway::operator()(const StopEvent& event) {
  LOG(INFO)("Stopping the gateway...");
  _web_socket.connection(event);
  _rest.connection(event);
}

void Gateway::operator()(const TimerEvent& event) {
  _web_socket.connection(event);
  _rest.connection(event);
  _base.loop(EVLOOP_NONBLOCK);
}

void Gateway::operator()(const ConnectionStatusEvent&) {
}

void Gateway::operator()(
    const CreateOrderEvent& event,
    const std::string_view& request_id,
    uint32_t gateway_order_id) {
}

void Gateway::operator()(
    const ModifyOrderEvent&,
    const std::string_view&,
    const server::OMS_Order& order) {
  throw server::OMS_Exception(
      Error::MODIFY_ORDER_NOT_SUPPORTED,
      order);
}

void Gateway::operator()(
    const CancelOrderEvent& event,
    const std::string_view& request_id,
    const server::OMS_Order& order) {
}

void Gateway::operator()(Metrics& metrics) {
  _web_socket.connection(metrics);
  _rest.connection(metrics);
}

// ws

void Gateway::operator()(const WebSocket& web_socket) {
  if (web_socket.ready()) {
    _rest.download.begin();
  } else {
    _rest.download.reset();
    _symbols.clear();
  }
}

// rest

void Gateway::operator()(const Rest&) {
  if (_rest.connection.ready())
    _rest.download.bump();
}


// UTILS:

void Gateway::update_market_data(GatewayStatus gateway_status) {
  if (gateway_status == _market_data_status)
    return;
  _market_data_status = gateway_status;
  MarketDataStatus market_data_status {
    .status = _market_data_status,
  };
  enqueue(
      market_data_status,
      true);
  LOG(INFO)(FMT_STRING("market_data_status={}"), _market_data_status);
}

void Gateway::update_order_manager(GatewayStatus gateway_status) {
  if (gateway_status == _order_manager_status)
    return;
  _order_manager_status = gateway_status;
  OrderManagerStatus order_manager_status {
    .account = _account,
    .status = _order_manager_status,
  };
  enqueue(
      order_manager_status,
      true);
  LOG(INFO)(FMT_STRING("order_manager_status={}"), _order_manager_status);
}

uint32_t Gateway::download(RestDownload::State state) {
  if (_web_socket.connection.ready() == false)
    return -1;
  switch (state) {
    case RestDownload::State::UNDEFINED:
      assert(false);
      break;
    case RestDownload::State::EXCHANGE_INFO:
      download_exchange_info();
      return 1;
    case RestDownload::State::DONE:
      // update(GatewayStatus::READY);
      return 0;
  }
  assert(false);
  return 0;
}

void Gateway::download_exchange_info() {
  constexpr auto state = RestDownload::State::EXCHANGE_INFO;
  _rest.connection.get<json::ExchangeInfo>(
      [this](auto& promise) {
    try {
      (*this)(promise.get());
      _rest.download.check(state);
    } catch (NetworkError&) {
      _rest.download.retry(state);
    }
  });
}

/*
void Gateway::begin_download() {
  assert(_download == Download::NONE);
  assert(_order_manager_status == GatewayStatus::LOGIN_SENT);
  assert(_market_data_status == GatewayStatus::LOGIN_SENT);
  update_order_manager(GatewayStatus::DOWNLOADING);
  update_market_data(GatewayStatus::DOWNLOADING);
  LOG(INFO)("Download:");
  subscribe_instrument();
}

void Gateway::check_download() {
  assert(_download != Download::NONE);
  assert(_download != Download::READY);
  switch (_download) {
    case Download::NONE:
      assert(false);
      break;
    case Download::ACCOUNTS: {
      LOG(INFO)("Download accounts COMPLETED");
      update_order_manager(GatewayStatus::READY);
      subscribe_order_book_l2();
      break;
    }
    case Download::PRODUCTS: {
      LOG(INFO)("Download products COMPLETED");
      // download_accounts();
      update_order_manager(GatewayStatus::READY);
      subscribe_order_book_l2();
      break;
    }
    case Download::ORDER_BOOKS: {
      LOG(INFO)("Download order books COMPLETED");
      update_market_data(GatewayStatus::READY);
      LOG(INFO)("Download COMPLETED");
      _download = Download::READY;
      break;
    }
    case Download::READY:
      assert(false);
      break;
  }
}

void Gateway::download_accounts() {
  assert(_download != Download::READY);
  LOG(INFO)("Download accounts...");
  // _rest.get_accounts();
  _download = Download::ACCOUNTS;
}

void Gateway::subscribe_instrument() {
  assert(_download != Download::READY);
  LOG(INFO)("Download products...");
  // XXX _rest.get_products();
  _web_socket.subscribe("instrument");
  _download = Download::PRODUCTS;
}

void Gateway::subscribe_order_book_l2() {
  assert(_download != Download::READY);
  LOG(INFO)("Download order books");
  _web_socket.subscribe("orderBookL2", _symbols);
  _download = Download::ORDER_BOOKS;

  // XXX not here
  _web_socket.subscribe("funding", _symbols);
  _web_socket.subscribe("liquidation", _symbols);
  _web_socket.subscribe("quote", _symbols);
  _web_socket.subscribe("settlement", _symbols);
  _web_socket.subscribe("trade", _symbols);
  // XXX private
  _web_socket.subscribe("execution", _symbols);
  _web_socket.subscribe("order", _symbols);
  _web_socket.subscribe("margin", _symbols);
  _web_socket.subscribe("position", _symbols);
  // XXX other
  // cancelAllAfter
  // authKeyExpires
}
*/

void Gateway::operator()(const json::ExchangeInfo&) {
}

template <typename T>
void Gateway::enqueue(
    const T& event,
    bool is_last) {
  auto now = core::get_system_clock();
  _dispatcher(
      event,
      now,
      now,
      is_last);
}

template <typename T>
void Gateway::enqueue(
    uint8_t user_id,
    const T& event,
    bool is_last) {
  auto now = core::get_system_clock();
  _dispatcher(
      user_id,
      event,
      now,
      now,
      is_last);
}

}  // namespace binance
}  // namespace roq
