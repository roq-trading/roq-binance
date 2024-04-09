/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/order_entry_2.hpp"

#include <tuple>
#include <utility>

#include "roq/mask.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/utils/charconv.hpp"
#include "roq/utils/compare.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/binance/json/error.hpp"
#include "roq/binance/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace binance {

// === CONSTANTS ===

namespace {
auto const NAME = "om2"sv;

auto const SUPPORTS = Mask{
    SupportType::FUNDS,
};
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

struct create_metrics final : public core::metrics::Factory {
  create_metrics(auto &settings, auto const &group, auto const &function)
      : core::metrics::Factory(settings.app.name, group, function) {}
  create_metrics(auto &settings, auto const &group, auto const &function, auto const &params)
      : core::metrics::Factory(settings.app.name, group, function, params) {}
};

auto get_retry_after(auto &response) {
  std::chrono::nanoseconds result = {};
  response.dispatch(web::http::Header::RETRY_AFTER, [&](auto &value) {
    try {
      // XXX FIXME could also be a datetime (see https://datatracker.ietf.org/doc/html/rfc7231)
      auto seconds = utils::from_string_relaxed<int64_t>(value);
      result = std::chrono::seconds{seconds};
    } catch (RuntimeError &) {
      log::warn<5>(R"(Failed to parse text="{}")"sv, value);
    }
  });
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

OrderEntry2::OrderEntry2(
    Handler &handler, io::Context &context, uint16_t stream_id, Account &account, Shared &shared, Request &request)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, account.name)},
      connection_{create_connection(*this, shared.settings, context)},
      decode_buffer_(shared.settings.misc.decode_buffer_size),
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .listen_key = create_metrics(shared.settings, name_, "listen_key"sv),
          .listen_key_ack = create_metrics(shared.settings, name_, "listen_key_ack"sv),
          .account = create_metrics(shared.settings, name_, "account"sv),
          .account_ack = create_metrics(shared.settings, name_, "account_ack"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
      },
      rate_limiter_{
          .requests_1m = create_metrics(shared.settings, name_, "requests"sv, "1m"sv),
      },
      account_{account}, shared_{shared}, request_{request},
      download_{shared.settings.rest.request_timeout, [this](auto state) { return download(state); }} {
}

void OrderEntry2::operator()(Event<Start> const &) {
  (*connection_).start();
}

void OrderEntry2::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void OrderEntry2::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  refresh_listen_key(now);
  if (ready() && !downloading()) {
    if (!downloading() && request_.respond_account < request_.request_account) {
      log::info("Download account..."sv);
      get_account();
      download_account_ = true;
    }
  }
}

void OrderEntry2::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.listen_key, metrics::Type::PROFILE)
      .write(profile_.listen_key_ack, metrics::Type::PROFILE)
      .write(profile_.account, metrics::Type::PROFILE)
      .write(profile_.account_ack, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      // rate limiter
      .write(rate_limiter_.requests_1m, metrics::Type::RATE_LIMITER);
}

void OrderEntry2::operator()(Event<Disconnected> const &event) {
  auto user_id = event.message_info.source;
  account_.cancel_order_request_buffer_[user_id].reset();
}

void OrderEntry2::operator()(Trace<web::rest::Client::Connected> const &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void OrderEntry2::operator()(Trace<web::rest::Client::Disconnected> const &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading())
    download_.reset();
  download_account_ = false;
}

void OrderEntry2::operator()(Trace<web::rest::Client::Header> const &event) {
  auto &header = event.value;
  if (utils::case_insensitive_compare(header.name, "x-mbx-used-weight-1m"sv) == 0) {
    try {
      auto value = utils::from_string_relaxed<int64_t>(header.value);
      rate_limiter_.requests_1m.set(value);
    } catch (RuntimeError &) {
      log::warn<5>(R"(Failed to parse text="{}")"sv, header.value);
    }
  }
}

void OrderEntry2::operator()(Trace<web::rest::Client::Latency> const &event) {
  auto &[trace_info, latency] = event;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = account_.name,
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntry2::operator()(ConnectionStatus status) {
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

uint32_t OrderEntry2::download(OrderEntryState state) {
  switch (state) {
    using enum OrderEntryState;
    case UNDEFINED:
      assert(false);
      break;
    case LISTEN_KEY:
      get_listen_key();
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

void OrderEntry2::get_listen_key() {
  profile_.listen_key([&]() {
    auto headers = account_.create_headers();
    auto request = web::rest::Request{
        .method = web::http::Method::POST,
        .path = shared_.api.simple.user_data_stream,
        .query = {},
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_listen_key_ack(event);
    };
    (*connection_)("listen_key"sv, request, callback);
  });
}

void OrderEntry2::get_listen_key_ack(Trace<web::rest::Response> const &event) {
  auto constexpr const STATE = OrderEntryState::LISTEN_KEY;
  profile_.listen_key_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::ListenKey listen_key{body};
      Trace event_2{event, listen_key};
      (*this)(event_2);
      download_.check_relaxed(STATE);
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      if (download_.downloading())
        download_.retry(STATE);
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntry2::operator()(Trace<json::ListenKey> const &event) {
  auto &[trace_info, listen_key] = event;
  log::info<2>("listen_key={}"sv, listen_key);
  bool initial = std::empty(listen_key_);
  if (utils::update(listen_key_, listen_key.listen_key)) {
    if (initial) {
      log::info<1>(R"(Listen key has been acquired (value="{}"))"sv, listen_key_);
      auto listen_key_update = ListenKeyUpdate{
          .account = account_.name,
          .listen_key = listen_key.listen_key,
      };
      create_trace_and_dispatch(handler_, trace_info, listen_key_update);
    } else {
      log::info<1>("Listen key has been refreshed!"sv);
    }
  }
  auto now = clock::get_system();
  listen_key_refresh_ = now + shared_.settings.rest.listen_key_refresh;
}

// account

void OrderEntry2::get_account() {
  profile_.account([&]() {
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto query = account_.create_query(now);
    auto headers = account_.create_headers();
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = shared_.api.simple.account,
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
      get_account_ack(event);
    };
    (*connection_)("account"sv, request, callback);
  });
}

void OrderEntry2::get_account_ack(Trace<web::rest::Response> const &event) {
  profile_.account_ack([&]() {
    auto handle_success = [&](auto &body) {
      json::Account account{body, decode_buffer_};
      Trace event_2{event, account};
      (*this)(event_2);
      request_.respond_account = clock::get_system();  // completion
      download_account_ = false;
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      request_.respond_account = clock::get_system();  // completion
      download_account_ = false;
    };
    process_response(event, handle_success, handle_error);
  });
}

void OrderEntry2::operator()(Trace<json::Account> const &event) {
  auto &[trace_info, account] = event;
  log::info("DEBUG account={}"sv, account);
  log::info<2>("account={}"sv, account);
  for (auto &item : account.balances) {
    log::info("DEBUG item={}"sv, item);
    auto funds_update = FundsUpdate{
        .stream_id = stream_id_,
        .account = account_.name,
        .currency = item.asset,
        .margin_mode = {},
        .balance = item.free,
        .hold = item.locked,
        .external_account = {},
        .update_type = UpdateType::SNAPSHOT,
        .exchange_time_utc = account.update_time,
        .sending_time_utc = account.update_time,
    };
    create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
}

// ...

void OrderEntry2::refresh_listen_key(std::chrono::nanoseconds now) {
  if (!ready_)
    return;
  if (listen_key_refresh_ == listen_key_refresh_.zero() || now < listen_key_refresh_)
    return;
  log::info<1>("Refreshing listen key..."sv);
  listen_key_refresh_ = now + shared_.settings.rest.listen_key_refresh;
  get_listen_key();
}

// helpers

template <typename SuccessHandler, typename ErrorHandler>
void OrderEntry2::process_response(
    web::rest::Response const &response, SuccessHandler success_handler, ErrorHandler error_handler) {
  try {
    auto [status, category, body] = response.result();
    switch (category) {
      using enum web::http::Category;
      case SUCCESS:  // 2xx
        success_handler(body);
        break;
      case CLIENT_ERROR:  // 4xx
        switch (status) {
          using enum web::http::Status;
          case FORBIDDEN:           // 403
            waf_limit_violation();  // note! this is *very* serious
            [[fallthrough]];
          case I_AM_A_TEAPOT:        // 418
          case TOO_MANY_REQUESTS: {  // 429
            auto text = fmt::format("{}"sv, status);
            error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::REQUEST_RATE_LIMIT_REACHED, text);
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
      case SERVER_ERROR: {  // 5xx
        auto text = fmt::format("{}"sv, status);
        error_handler(Origin::EXCHANGE, RequestStatus::ERROR, Error::UNKNOWN, text);
        break;
      }
      default:
        response.expect(web::http::Status::OK);  // throws
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

void OrderEntry2::test(web::http::Status status) {
  if (status != web::http::Status::FORBIDDEN) [[likely]]
    return;
  waf_limit_violation();
}

void OrderEntry2::waf_limit_violation() {
  if (shared_.settings.rest.terminate_on_403) {
    log::fatal("WAF limit violation"sv);
  } else {
    log::warn("WAF limit violation"sv);
    (*connection_).suspend(shared_.settings.rest.back_off_delay);
  }
}

}  // namespace binance
}  // namespace roq
