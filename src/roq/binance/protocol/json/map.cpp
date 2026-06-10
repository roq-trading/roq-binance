/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/protocol/json/map.hpp"

using namespace std::literals;

namespace roq {

namespace {
template <typename... Args>
using Helper = detail::MapHelper<Args...>;
}

// binance ==> roq

// binance::protocol::json::OrderStatus ==> roq::OrderStatus

template <>
template <>
constexpr Helper<binance::protocol::json::OrderStatus>::operator std::optional<roq::OrderStatus>() const {
  switch (std::get<0>(args_)) {
    using enum binance::protocol::json::OrderStatus::type_t;
    case UNDEFINED_INTERNAL:
      return roq::OrderStatus::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::OrderStatus::UNDEFINED;
    case NEW:
      return roq::OrderStatus::WORKING;
    case PARTIALLY_FILLED:
      return roq::OrderStatus::WORKING;
    case FILLED:
      return roq::OrderStatus::COMPLETED;
    case CANCELED:
      return roq::OrderStatus::CANCELED;
    case PENDING_CANCEL:  // XXX HANS what do do?
      return roq::OrderStatus::UNDEFINED;
    case REJECTED:
      return roq::OrderStatus::REJECTED;
    case EXPIRED:
      return roq::OrderStatus::EXPIRED;
  }
  return {};
}

static_assert(Helper{binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL}} == roq::OrderStatus::UNDEFINED);
static_assert(Helper{binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::NEW}} == roq::OrderStatus::WORKING);
static_assert(Helper{binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::PARTIALLY_FILLED}} == roq::OrderStatus::WORKING);
static_assert(Helper{binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::FILLED}} == roq::OrderStatus::COMPLETED);
static_assert(Helper{binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::CANCELED}} == roq::OrderStatus::CANCELED);
static_assert(Helper{binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::PENDING_CANCEL}} == roq::OrderStatus::UNDEFINED);
static_assert(Helper{binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::REJECTED}} == roq::OrderStatus::REJECTED);
static_assert(Helper{binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::EXPIRED}} == roq::OrderStatus::EXPIRED);

template <>
template <>
std::optional<roq::OrderStatus> Map<binance::protocol::json::OrderStatus>::helper() const {
  return Helper{args_};
}

// binance::protocol::json::OrderType ==> roq::OrderType

template <>
template <>
constexpr Helper<binance::protocol::json::OrderType>::operator std::optional<roq::OrderType>() const {
  switch (std::get<0>(args_)) {
    using enum binance::protocol::json::OrderType::type_t;
    case UNDEFINED_INTERNAL:
      return roq::OrderType::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::OrderType::UNDEFINED;
    case LIMIT:
      return roq::OrderType::LIMIT;
    case MARKET:
      return roq::OrderType::MARKET;
    case STOP_LOSS:
      return roq::OrderType::MARKET;
    case STOP_LOSS_LIMIT:
      return roq::OrderType::LIMIT;
    case TAKE_PROFIT:
      return roq::OrderType::UNDEFINED;
    case TAKE_PROFIT_LIMIT:
      return roq::OrderType::UNDEFINED;
    case LIMIT_MAKER:
      return roq::OrderType::LIMIT;
  }
  return {};
}

static_assert(Helper{binance::protocol::json::OrderType{binance::protocol::json::OrderType::UNDEFINED_INTERNAL}} == roq::OrderType::UNDEFINED);
static_assert(Helper{binance::protocol::json::OrderType{binance::protocol::json::OrderType::LIMIT}} == roq::OrderType::LIMIT);
static_assert(Helper{binance::protocol::json::OrderType{binance::protocol::json::OrderType::MARKET}} == roq::OrderType::MARKET);
static_assert(Helper{binance::protocol::json::OrderType{binance::protocol::json::OrderType::STOP_LOSS}} == roq::OrderType::MARKET);
static_assert(Helper{binance::protocol::json::OrderType{binance::protocol::json::OrderType::STOP_LOSS_LIMIT}} == roq::OrderType::LIMIT);
static_assert(Helper{binance::protocol::json::OrderType{binance::protocol::json::OrderType::TAKE_PROFIT}} == roq::OrderType::UNDEFINED);
static_assert(Helper{binance::protocol::json::OrderType{binance::protocol::json::OrderType::TAKE_PROFIT_LIMIT}} == roq::OrderType::UNDEFINED);
static_assert(Helper{binance::protocol::json::OrderType{binance::protocol::json::OrderType::LIMIT_MAKER}} == roq::OrderType::LIMIT);

template <>
template <>
std::optional<roq::OrderType> Map<binance::protocol::json::OrderType>::helper() const {
  return Helper{args_};
}

// binance::protocol::json::Side ==> roq::Side

template <>
template <>
constexpr Helper<binance::protocol::json::Side>::operator std::optional<roq::Side>() const {
  switch (std::get<0>(args_)) {
    using enum binance::protocol::json::Side::type_t;
    case UNDEFINED_INTERNAL:
      return roq::Side::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::Side::UNDEFINED;
    case BUY:
      return roq::Side::BUY;
    case SELL:
      return roq::Side::SELL;
  }
  return {};
}

static_assert(Helper{binance::protocol::json::Side{binance::protocol::json::Side::UNDEFINED_INTERNAL}} == roq::Side::UNDEFINED);
static_assert(Helper{binance::protocol::json::Side{binance::protocol::json::Side::BUY}} == roq::Side::BUY);
static_assert(Helper{binance::protocol::json::Side{binance::protocol::json::Side::SELL}} == roq::Side::SELL);

template <>
template <>
std::optional<roq::Side> Map<binance::protocol::json::Side>::helper() const {
  return Helper{args_};
}

// binance::protocol::json::SymbolStatus ==> roq::TradingStatus

template <>
template <>
constexpr Helper<binance::protocol::json::SymbolStatus>::operator std::optional<roq::TradingStatus>() const {
  switch (std::get<0>(args_)) {
    using enum binance::protocol::json::SymbolStatus::type_t;
    case UNDEFINED_INTERNAL:
      return roq::TradingStatus::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::TradingStatus::UNDEFINED;
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
  return {};
}

static_assert(Helper{binance::protocol::json::SymbolStatus{binance::protocol::json::SymbolStatus::UNDEFINED_INTERNAL}} == roq::TradingStatus::UNDEFINED);
static_assert(Helper{binance::protocol::json::SymbolStatus{binance::protocol::json::SymbolStatus::TRADING}} == roq::TradingStatus::OPEN);
static_assert(Helper{binance::protocol::json::SymbolStatus{binance::protocol::json::SymbolStatus::HALT}} == roq::TradingStatus::HALT);
static_assert(Helper{binance::protocol::json::SymbolStatus{binance::protocol::json::SymbolStatus::BREAK}} == roq::TradingStatus::CLOSE);
static_assert(Helper{binance::protocol::json::SymbolStatus{binance::protocol::json::SymbolStatus::END_OF_DAY}} == roq::TradingStatus::END_OF_DAY);
static_assert(Helper{binance::protocol::json::SymbolStatus{binance::protocol::json::SymbolStatus::PRE_TRADING}} == roq::TradingStatus::PRE_OPEN);
static_assert(Helper{binance::protocol::json::SymbolStatus{binance::protocol::json::SymbolStatus::AUCTION_MATCH}} == roq::TradingStatus::PRE_OPEN);
static_assert(Helper{binance::protocol::json::SymbolStatus{binance::protocol::json::SymbolStatus::POST_TRADING}} == roq::TradingStatus::CLOSE);

template <>
template <>
std::optional<roq::TradingStatus> Map<binance::protocol::json::SymbolStatus>::helper() const {
  return Helper{args_};
}

// binance::protocol::json::TimeInForce ==> roq::TimeInForce

template <>
template <>
constexpr Helper<binance::protocol::json::TimeInForce>::operator std::optional<roq::TimeInForce>() const {
  switch (std::get<0>(args_)) {
    using enum binance::protocol::json::TimeInForce::type_t;
    case UNDEFINED_INTERNAL:
      return roq::TimeInForce::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::TimeInForce::UNDEFINED;
    case GTC:
      return roq::TimeInForce::GTC;
    case IOC:
      return roq::TimeInForce::IOC;
    case FOK:
      return roq::TimeInForce::FOK;
  }
  return {};
}

static_assert(Helper{binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL}} == roq::TimeInForce::UNDEFINED);
static_assert(Helper{binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::GTC}} == roq::TimeInForce::GTC);
static_assert(Helper{binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::IOC}} == roq::TimeInForce::IOC);
static_assert(Helper{binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::FOK}} == roq::TimeInForce::FOK);

template <>
template <>
std::optional<roq::TimeInForce> Map<binance::protocol::json::TimeInForce>::helper() const {
  return Helper{args_};
}

// roq ==> binance::protocol::json

// roq::OrderStatus ==> binance::protocol::json::OrderStatus

template <>
template <>
constexpr Helper<roq::OrderStatus>::operator std::optional<binance::protocol::json::OrderStatus>() const {
  switch (std::get<0>(args_)) {
    using enum roq::OrderStatus;
    case UNDEFINED:
      return binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL;
    case SENT:
      return binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL;
    case ACCEPTED:
      return binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL;
    case SUSPENDED:
      return binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL;
    case WORKING:
      return binance::protocol::json::OrderStatus::NEW;
    case STOPPED:
      return binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL;
    case COMPLETED:
      return binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL;  // XXX HANS no enum for COMPLETED ???
    case EXPIRED:
      return binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL;
    case CANCELED:
      return binance::protocol::json::OrderStatus::CANCELED;
    case REJECTED:
      return binance::protocol::json::OrderStatus::REJECTED;
  }
  return {};
}

static_assert(Helper{roq::OrderStatus::UNDEFINED} == binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::SENT} == binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::ACCEPTED} == binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::SUSPENDED} == binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::WORKING} == binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::NEW});
static_assert(Helper{roq::OrderStatus::STOPPED} == binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::COMPLETED} == binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::EXPIRED} == binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::CANCELED} == binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::CANCELED});
static_assert(Helper{roq::OrderStatus::REJECTED} == binance::protocol::json::OrderStatus{binance::protocol::json::OrderStatus::REJECTED});

template <>
template <>
std::optional<binance::protocol::json::OrderStatus> Map<roq::OrderStatus>::helper() const {
  return Helper{args_};
}

// roq::OrderType ==> binance::protocol::json::OrderType

template <>
template <>
constexpr Helper<roq::OrderType>::operator std::optional<binance::protocol::json::OrderType>() const {
  switch (std::get<0>(args_)) {
    using enum roq::OrderType;
    case UNDEFINED:
      return binance::protocol::json::OrderType::UNDEFINED_INTERNAL;
    case MARKET:
      return binance::protocol::json::OrderType::MARKET;
    case LIMIT:
      return binance::protocol::json::OrderType::LIMIT;
  }
  return {};
}

static_assert(Helper{roq::OrderType::UNDEFINED} == binance::protocol::json::OrderType{binance::protocol::json::OrderType::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderType::MARKET} == binance::protocol::json::OrderType{binance::protocol::json::OrderType::MARKET});
static_assert(Helper{roq::OrderType::LIMIT} == binance::protocol::json::OrderType{binance::protocol::json::OrderType::LIMIT});

template <>
template <>
std::optional<binance::protocol::json::OrderType> Map<roq::OrderType>::helper() const {
  return Helper{args_};
}

// roq::Side ==> binance::protocol::json::Side

template <>
template <>
constexpr Helper<roq::Side>::operator std::optional<binance::protocol::json::Side>() const {
  switch (std::get<0>(args_)) {
    using enum roq::Side;
    case UNDEFINED:
      return binance::protocol::json::Side::UNDEFINED_INTERNAL;
    case BUY:
      return binance::protocol::json::Side::BUY;
    case SELL:
      return binance::protocol::json::Side::SELL;
  }
  return {};
}

static_assert(Helper{roq::Side::UNDEFINED} == binance::protocol::json::Side{binance::protocol::json::Side::UNDEFINED_INTERNAL});
static_assert(Helper{roq::Side::BUY} == binance::protocol::json::Side{binance::protocol::json::Side::BUY});
static_assert(Helper{roq::Side::SELL} == binance::protocol::json::Side{binance::protocol::json::Side::SELL});

template <>
template <>
std::optional<binance::protocol::json::Side> Map<roq::Side>::helper() const {
  return Helper{args_};
}

// roq::TimeInForce ==> binance::protocol::json::TimeInForce

template <>
template <>
constexpr Helper<roq::TimeInForce>::operator std::optional<binance::protocol::json::TimeInForce>() const {
  switch (std::get<0>(args_)) {
    using enum roq::TimeInForce;
    case UNDEFINED:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
    case GFD:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
    case GTC:
      return binance::protocol::json::TimeInForce::GTC;
    case OPG:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
    case IOC:
      return binance::protocol::json::TimeInForce::IOC;
    case FOK:
      return binance::protocol::json::TimeInForce::FOK;
    case GTX:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
    case GTD:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
    case AT_THE_CLOSE:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
    case GOOD_THROUGH_CROSSING:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
    case AT_CROSSING:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
    case GOOD_FOR_TIME:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
    case GFA:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
    case GFM:
      return binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL;
  }
  return {};
}

static_assert(Helper{roq::TimeInForce::UNDEFINED} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GFD} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GTC} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::GTC});
static_assert(Helper{roq::TimeInForce::OPG} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::IOC} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::IOC});
static_assert(Helper{roq::TimeInForce::FOK} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::FOK});
static_assert(Helper{roq::TimeInForce::GTX} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GTD} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::AT_THE_CLOSE} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(
    Helper{roq::TimeInForce::GOOD_THROUGH_CROSSING} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::AT_CROSSING} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GOOD_FOR_TIME} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GFA} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GFM} == binance::protocol::json::TimeInForce{binance::protocol::json::TimeInForce::UNDEFINED_INTERNAL});

template <>
template <>
std::optional<binance::protocol::json::TimeInForce> Map<roq::TimeInForce>::helper() const {
  return Helper{args_};
}

}  // namespace roq
