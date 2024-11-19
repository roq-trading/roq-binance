/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/binance/json/map.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

// === HELPERS ===

namespace {
// note! constexpr helper for static testing
template <typename... Args>
struct Helper final {
  explicit constexpr Helper(std::tuple<Args...> const &args) : args_{args} {}
  explicit constexpr Helper(Args &&...args_) : args_{std::forward<Args>(args_)...} {}

  template <typename R>
  constexpr operator R();

 private:
  std::tuple<Args...> const args_;
};

// ==> roq

// OrderStatus ==> roq::OrderStatus

template <>
template <>
constexpr Helper<OrderStatus>::operator roq::OrderStatus() {
  switch (std::get<0>(args_)) {
    using enum json::OrderStatus::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
    case NEW:
      return roq::OrderStatus::WORKING;
    case PARTIALLY_FILLED:
      return roq::OrderStatus::WORKING;
    case FILLED:
      return roq::OrderStatus::COMPLETED;
    case CANCELED:
      return roq::OrderStatus::CANCELED;
    case PENDING_CANCEL:  // XXX HANS what do do?
      return {};          // note!
    case REJECTED:
      return roq::OrderStatus::REJECTED;
    case EXPIRED:
      return roq::OrderStatus::EXPIRED;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::OrderStatus>(Helper{OrderStatus{OrderStatus::UNDEFINED__}}) == roq::OrderStatus::UNDEFINED);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderStatus{OrderStatus::NEW}}) == roq::OrderStatus::WORKING);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderStatus{OrderStatus::PARTIALLY_FILLED}}) == roq::OrderStatus::WORKING);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderStatus{OrderStatus::FILLED}}) == roq::OrderStatus::COMPLETED);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderStatus{OrderStatus::CANCELED}}) == roq::OrderStatus::CANCELED);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderStatus{OrderStatus::PENDING_CANCEL}}) == roq::OrderStatus::UNDEFINED);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderStatus{OrderStatus::REJECTED}}) == roq::OrderStatus::REJECTED);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderStatus{OrderStatus::EXPIRED}}) == roq::OrderStatus::EXPIRED);

// OrderType ==> roq::OrderType

template <>
template <>
constexpr Helper<OrderType>::operator roq::OrderType() {
  switch (std::get<0>(args_)) {
    using enum json::OrderType::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
    case LIMIT:
      return roq::OrderType::LIMIT;
    case MARKET:
      return roq::OrderType::MARKET;
    case STOP_LOSS:
      return {};  // note!
    case STOP_LOSS_LIMIT:
      return {};  // note!
    case TAKE_PROFIT:
      return {};  // note!
    case TAKE_PROFIT_LIMIT:
      return {};  // note!
    case LIMIT_MAKER:
      return roq::OrderType::LIMIT;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::OrderType>(Helper{OrderType{OrderType::UNDEFINED__}}) == roq::OrderType::UNDEFINED);
static_assert(static_cast<roq::OrderType>(Helper{OrderType{OrderType::LIMIT}}) == roq::OrderType::LIMIT);
static_assert(static_cast<roq::OrderType>(Helper{OrderType{OrderType::MARKET}}) == roq::OrderType::MARKET);
static_assert(static_cast<roq::OrderType>(Helper{OrderType{OrderType::STOP_LOSS}}) == roq::OrderType::UNDEFINED);
static_assert(static_cast<roq::OrderType>(Helper{OrderType{OrderType::STOP_LOSS_LIMIT}}) == roq::OrderType::UNDEFINED);
static_assert(static_cast<roq::OrderType>(Helper{OrderType{OrderType::TAKE_PROFIT}}) == roq::OrderType::UNDEFINED);
static_assert(static_cast<roq::OrderType>(Helper{OrderType{OrderType::TAKE_PROFIT_LIMIT}}) == roq::OrderType::UNDEFINED);
static_assert(static_cast<roq::OrderType>(Helper{OrderType{OrderType::LIMIT_MAKER}}) == roq::OrderType::LIMIT);

// Side ==> roq::Side

template <>
template <>
constexpr Helper<Side>::operator roq::Side() {
  switch (std::get<0>(args_)) {
    using enum json::Side::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
    case BUY:
      return roq::Side::BUY;
    case SELL:
      return roq::Side::SELL;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::Side>(Helper{Side{Side::UNDEFINED__}}) == roq::Side::UNDEFINED);
static_assert(static_cast<roq::Side>(Helper{Side{Side::BUY}}) == roq::Side::BUY);
static_assert(static_cast<roq::Side>(Helper{Side{Side::SELL}}) == roq::Side::SELL);

// SymbolStatus ==> roq::Side

template <>
template <>
constexpr Helper<SymbolStatus>::operator roq::TradingStatus() {
  switch (std::get<0>(args_)) {
    using enum json::SymbolStatus::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
    case TRADING:
      return roq::TradingStatus::OPEN;
    case HALT:
      return roq::TradingStatus::HALT;
    case BREAK:
      return roq::TradingStatus::CLOSE;
    case END_OF_DAY:
      return roq::TradingStatus::END_OF_DAY;
      // note! following probably not used
      // - https://dev.binance.vision/t/explanation-on-symbol-status/118
    case PRE_TRADING:
      return roq::TradingStatus::PRE_OPEN;
    case AUCTION_MATCH:
      return roq::TradingStatus::PRE_OPEN;
    case POST_TRADING:
      return roq::TradingStatus::CLOSE;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::TradingStatus>(Helper{SymbolStatus{SymbolStatus::UNDEFINED__}}) == roq::TradingStatus::UNDEFINED);
static_assert(static_cast<roq::TradingStatus>(Helper{SymbolStatus{SymbolStatus::TRADING}}) == roq::TradingStatus::OPEN);
static_assert(static_cast<roq::TradingStatus>(Helper{SymbolStatus{SymbolStatus::HALT}}) == roq::TradingStatus::HALT);
static_assert(static_cast<roq::TradingStatus>(Helper{SymbolStatus{SymbolStatus::BREAK}}) == roq::TradingStatus::CLOSE);
static_assert(static_cast<roq::TradingStatus>(Helper{SymbolStatus{SymbolStatus::END_OF_DAY}}) == roq::TradingStatus::END_OF_DAY);
static_assert(static_cast<roq::TradingStatus>(Helper{SymbolStatus{SymbolStatus::PRE_TRADING}}) == roq::TradingStatus::PRE_OPEN);
static_assert(static_cast<roq::TradingStatus>(Helper{SymbolStatus{SymbolStatus::AUCTION_MATCH}}) == roq::TradingStatus::PRE_OPEN);
static_assert(static_cast<roq::TradingStatus>(Helper{SymbolStatus{SymbolStatus::POST_TRADING}}) == roq::TradingStatus::CLOSE);

// TimeInForce ==> roq::TimeInForce

template <>
template <>
constexpr Helper<TimeInForce>::operator roq::TimeInForce() {
  switch (std::get<0>(args_)) {
    using enum json::TimeInForce::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
    case GTC:
      return roq::TimeInForce::GTC;
    case IOC:
      return roq::TimeInForce::IOC;
    case FOK:
      return roq::TimeInForce::FOK;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::TimeInForce>(Helper{TimeInForce{TimeInForce::UNDEFINED__}}) == roq::TimeInForce::UNDEFINED);
static_assert(static_cast<roq::TimeInForce>(Helper{TimeInForce{TimeInForce::GTC}}) == roq::TimeInForce::GTC);
static_assert(static_cast<roq::TimeInForce>(Helper{TimeInForce{TimeInForce::IOC}}) == roq::TimeInForce::IOC);
static_assert(static_cast<roq::TimeInForce>(Helper{TimeInForce{TimeInForce::FOK}}) == roq::TimeInForce::FOK);

// roq ==>

// roq::Side ==> Side

template <>
template <>
constexpr Helper<roq::Side>::operator Side() {
  switch (std::get<0>(args_)) {
    using enum roq::Side;
    case UNDEFINED:
      return {};
    case BUY:
      return json::Side::BUY;
    case SELL:
      return json::Side::SELL;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<Side>(Helper{roq::Side::UNDEFINED}) == Side::UNDEFINED__);
static_assert(static_cast<Side>(Helper{roq::Side::BUY}) == Side::BUY);
static_assert(static_cast<Side>(Helper{roq::Side::SELL}) == Side::SELL);

// roq::OrderStatus ==> OrderStatus

template <>
template <>
constexpr Helper<roq::OrderStatus>::operator OrderStatus() {
  switch (std::get<0>(args_)) {
    using enum roq::OrderStatus;
    case UNDEFINED:
      return {};
    case SENT:
      return {};  // note!
    case ACCEPTED:
      return {};  // note!
    case SUSPENDED:
      return {};  // note!
    case WORKING:
      return json::OrderStatus::NEW;
    case STOPPED:
      return {};  // note!
    case COMPLETED:
      return {};  // XXX HANS no enum for COMPLETED ???
    case EXPIRED:
      return {};  // note!
    case CANCELED:
      return json::OrderStatus::CANCELED;
    case REJECTED:
      return json::OrderStatus::REJECTED;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<OrderStatus>(Helper{roq::OrderStatus::UNDEFINED}) == OrderStatus::UNDEFINED__);
static_assert(static_cast<OrderStatus>(Helper{roq::OrderStatus::SENT}) == OrderStatus::UNDEFINED__);
static_assert(static_cast<OrderStatus>(Helper{roq::OrderStatus::ACCEPTED}) == OrderStatus::UNDEFINED__);
static_assert(static_cast<OrderStatus>(Helper{roq::OrderStatus::SUSPENDED}) == OrderStatus::UNDEFINED__);
static_assert(static_cast<OrderStatus>(Helper{roq::OrderStatus::WORKING}) == OrderStatus::NEW);
static_assert(static_cast<OrderStatus>(Helper{roq::OrderStatus::STOPPED}) == OrderStatus::UNDEFINED__);
static_assert(static_cast<OrderStatus>(Helper{roq::OrderStatus::COMPLETED}) == OrderStatus::UNDEFINED__);
static_assert(static_cast<OrderStatus>(Helper{roq::OrderStatus::EXPIRED}) == OrderStatus::UNDEFINED__);
static_assert(static_cast<OrderStatus>(Helper{roq::OrderStatus::CANCELED}) == OrderStatus::CANCELED);
static_assert(static_cast<OrderStatus>(Helper{roq::OrderStatus::REJECTED}) == OrderStatus::REJECTED);

// roq::OrderType ==> OrderType

template <>
template <>
constexpr Helper<roq::OrderType>::operator OrderType() {
  switch (std::get<0>(args_)) {
    using enum roq::OrderType;
    case UNDEFINED:
      return {};
    case MARKET:
      return json::OrderType::MARKET;
    case LIMIT:
      return json::OrderType::LIMIT;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<OrderType>(Helper{roq::OrderType::UNDEFINED}) == OrderType::UNDEFINED__);
static_assert(static_cast<OrderType>(Helper{roq::OrderType::MARKET}) == OrderType::MARKET);
static_assert(static_cast<OrderType>(Helper{roq::OrderType::LIMIT}) == OrderType::LIMIT);

// roq::TimeInForce ==> TimeInForce

template <>
template <>
constexpr Helper<roq::TimeInForce>::operator TimeInForce() {
  switch (std::get<0>(args_)) {
    using enum roq::TimeInForce;
    case UNDEFINED:
      return {};
    case GFD:
      return {};  // note!
    case GTC:
      return json::TimeInForce::GTC;
    case OPG:
      return {};  // note!
    case IOC:
      return json::TimeInForce::IOC;
    case FOK:
      return json::TimeInForce::FOK;
    case GTX:
      return {};  // note!
    case GTD:
      return {};  // note!
    case AT_THE_CLOSE:
      return {};  // note!
    case GOOD_THROUGH_CROSSING:
      return {};  // note!
    case AT_CROSSING:
      return {};  // note!
    case GOOD_FOR_TIME:
      return {};  // note!
    case GFA:
      return {};  // note!
    case GFM:
      return {};  // note!
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::UNDEFINED}) == TimeInForce::UNDEFINED__);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::GFD}) == TimeInForce::UNDEFINED__);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::GTC}) == TimeInForce::GTC);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::OPG}) == TimeInForce::UNDEFINED__);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::IOC}) == TimeInForce::IOC);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::FOK}) == TimeInForce::FOK);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::GTX}) == TimeInForce::UNDEFINED__);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::GTD}) == TimeInForce::UNDEFINED__);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::AT_THE_CLOSE}) == TimeInForce::UNDEFINED__);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::GOOD_THROUGH_CROSSING}) == TimeInForce::UNDEFINED__);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::AT_CROSSING}) == TimeInForce::UNDEFINED__);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::GOOD_FOR_TIME}) == TimeInForce::UNDEFINED__);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::GFA}) == TimeInForce::UNDEFINED__);
static_assert(static_cast<TimeInForce>(Helper{roq::TimeInForce::GFM}) == TimeInForce::UNDEFINED__);
}  // namespace

// === IMPLEMENTATION ===

// ==> roq

template <>
template <>
Map<OrderStatus>::operator roq::OrderStatus() {
  return Helper{args_};
}

template <>
template <>
Map<OrderType>::operator roq::OrderType() {
  return Helper{args_};
}

template <>
template <>
Map<Side>::operator roq::Side() {
  return Helper{args_};
}

template <>
template <>
Map<SymbolStatus>::operator roq::TradingStatus() {
  return Helper{args_};
}

template <>
template <>
Map<TimeInForce>::operator roq::TimeInForce() {
  return Helper{args_};
}

// roq ==>

template <>
template <>
Map<roq::OrderStatus>::operator OrderStatus() {
  return Helper{args_};
}

template <>
template <>
Map<roq::OrderType>::operator OrderType() {
  return Helper{args_};
}

template <>
template <>
Map<roq::Side>::operator Side() {
  return Helper{args_};
}

template <>
template <>
Map<roq::TimeInForce>::operator TimeInForce() {
  return Helper{args_};
}

}  // namespace json
}  // namespace binance
}  // namespace roq
