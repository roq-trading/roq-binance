/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/json/map.hpp"

using namespace std::literals;

namespace roq {

namespace {
template <typename... Args>
using Helper = detail::MapHelper<Args...>;
}

// binance ==> roq

// binance::json::OrderStatus ==> roq::OrderStatus

template <>
template <>
constexpr Helper<binance::json::OrderStatus>::operator std::optional<roq::OrderStatus>() const {
  switch (std::get<0>(args_)) {
    using enum binance::json::OrderStatus::type_t;
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

static_assert(Helper{binance::json::OrderStatus{binance::json::OrderStatus::UNDEFINED_INTERNAL}} == roq::OrderStatus::UNDEFINED);
static_assert(Helper{binance::json::OrderStatus{binance::json::OrderStatus::NEW}} == roq::OrderStatus::WORKING);
static_assert(Helper{binance::json::OrderStatus{binance::json::OrderStatus::PARTIALLY_FILLED}} == roq::OrderStatus::WORKING);
static_assert(Helper{binance::json::OrderStatus{binance::json::OrderStatus::FILLED}} == roq::OrderStatus::COMPLETED);
static_assert(Helper{binance::json::OrderStatus{binance::json::OrderStatus::CANCELED}} == roq::OrderStatus::CANCELED);
static_assert(Helper{binance::json::OrderStatus{binance::json::OrderStatus::PENDING_CANCEL}} == roq::OrderStatus::UNDEFINED);
static_assert(Helper{binance::json::OrderStatus{binance::json::OrderStatus::REJECTED}} == roq::OrderStatus::REJECTED);
static_assert(Helper{binance::json::OrderStatus{binance::json::OrderStatus::EXPIRED}} == roq::OrderStatus::EXPIRED);

template <>
template <>
std::optional<roq::OrderStatus> Map<binance::json::OrderStatus>::helper() const {
  return Helper{args_};
}

// binance::json::OrderType ==> roq::OrderType

template <>
template <>
constexpr Helper<binance::json::OrderType>::operator std::optional<roq::OrderType>() const {
  switch (std::get<0>(args_)) {
    using enum binance::json::OrderType::type_t;
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

static_assert(Helper{binance::json::OrderType{binance::json::OrderType::UNDEFINED_INTERNAL}} == roq::OrderType::UNDEFINED);
static_assert(Helper{binance::json::OrderType{binance::json::OrderType::LIMIT}} == roq::OrderType::LIMIT);
static_assert(Helper{binance::json::OrderType{binance::json::OrderType::MARKET}} == roq::OrderType::MARKET);
static_assert(Helper{binance::json::OrderType{binance::json::OrderType::STOP_LOSS}} == roq::OrderType::MARKET);
static_assert(Helper{binance::json::OrderType{binance::json::OrderType::STOP_LOSS_LIMIT}} == roq::OrderType::LIMIT);
static_assert(Helper{binance::json::OrderType{binance::json::OrderType::TAKE_PROFIT}} == roq::OrderType::UNDEFINED);
static_assert(Helper{binance::json::OrderType{binance::json::OrderType::TAKE_PROFIT_LIMIT}} == roq::OrderType::UNDEFINED);
static_assert(Helper{binance::json::OrderType{binance::json::OrderType::LIMIT_MAKER}} == roq::OrderType::LIMIT);

template <>
template <>
std::optional<roq::OrderType> Map<binance::json::OrderType>::helper() const {
  return Helper{args_};
}

// binance::json::Side ==> roq::Side

template <>
template <>
constexpr Helper<binance::json::Side>::operator std::optional<roq::Side>() const {
  switch (std::get<0>(args_)) {
    using enum binance::json::Side::type_t;
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

static_assert(Helper{binance::json::Side{binance::json::Side::UNDEFINED_INTERNAL}} == roq::Side::UNDEFINED);
static_assert(Helper{binance::json::Side{binance::json::Side::BUY}} == roq::Side::BUY);
static_assert(Helper{binance::json::Side{binance::json::Side::SELL}} == roq::Side::SELL);

template <>
template <>
std::optional<roq::Side> Map<binance::json::Side>::helper() const {
  return Helper{args_};
}

// binance::json::SymbolStatus ==> roq::TradingStatus

template <>
template <>
constexpr Helper<binance::json::SymbolStatus>::operator std::optional<roq::TradingStatus>() const {
  switch (std::get<0>(args_)) {
    using enum binance::json::SymbolStatus::type_t;
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

static_assert(Helper{binance::json::SymbolStatus{binance::json::SymbolStatus::UNDEFINED_INTERNAL}} == roq::TradingStatus::UNDEFINED);
static_assert(Helper{binance::json::SymbolStatus{binance::json::SymbolStatus::TRADING}} == roq::TradingStatus::OPEN);
static_assert(Helper{binance::json::SymbolStatus{binance::json::SymbolStatus::HALT}} == roq::TradingStatus::HALT);
static_assert(Helper{binance::json::SymbolStatus{binance::json::SymbolStatus::BREAK}} == roq::TradingStatus::CLOSE);
static_assert(Helper{binance::json::SymbolStatus{binance::json::SymbolStatus::END_OF_DAY}} == roq::TradingStatus::END_OF_DAY);
static_assert(Helper{binance::json::SymbolStatus{binance::json::SymbolStatus::PRE_TRADING}} == roq::TradingStatus::PRE_OPEN);
static_assert(Helper{binance::json::SymbolStatus{binance::json::SymbolStatus::AUCTION_MATCH}} == roq::TradingStatus::PRE_OPEN);
static_assert(Helper{binance::json::SymbolStatus{binance::json::SymbolStatus::POST_TRADING}} == roq::TradingStatus::CLOSE);

template <>
template <>
std::optional<roq::TradingStatus> Map<binance::json::SymbolStatus>::helper() const {
  return Helper{args_};
}

// binance::json::TimeInForce ==> roq::TimeInForce

template <>
template <>
constexpr Helper<binance::json::TimeInForce>::operator std::optional<roq::TimeInForce>() const {
  switch (std::get<0>(args_)) {
    using enum binance::json::TimeInForce::type_t;
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

static_assert(Helper{binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL}} == roq::TimeInForce::UNDEFINED);
static_assert(Helper{binance::json::TimeInForce{binance::json::TimeInForce::GTC}} == roq::TimeInForce::GTC);
static_assert(Helper{binance::json::TimeInForce{binance::json::TimeInForce::IOC}} == roq::TimeInForce::IOC);
static_assert(Helper{binance::json::TimeInForce{binance::json::TimeInForce::FOK}} == roq::TimeInForce::FOK);

template <>
template <>
std::optional<roq::TimeInForce> Map<binance::json::TimeInForce>::helper() const {
  return Helper{args_};
}

// roq ==> binance::json

// roq::OrderStatus ==> binance::json::OrderStatus

template <>
template <>
constexpr Helper<roq::OrderStatus>::operator std::optional<binance::json::OrderStatus>() const {
  switch (std::get<0>(args_)) {
    using enum roq::OrderStatus;
    case UNDEFINED:
      return binance::json::OrderStatus::UNDEFINED_INTERNAL;
    case SENT:
      return binance::json::OrderStatus::UNDEFINED_INTERNAL;
    case ACCEPTED:
      return binance::json::OrderStatus::UNDEFINED_INTERNAL;
    case SUSPENDED:
      return binance::json::OrderStatus::UNDEFINED_INTERNAL;
    case WORKING:
      return binance::json::OrderStatus::NEW;
    case STOPPED:
      return binance::json::OrderStatus::UNDEFINED_INTERNAL;
    case COMPLETED:
      return binance::json::OrderStatus::UNDEFINED_INTERNAL;  // XXX HANS no enum for COMPLETED ???
    case EXPIRED:
      return binance::json::OrderStatus::UNDEFINED_INTERNAL;
    case CANCELED:
      return binance::json::OrderStatus::CANCELED;
    case REJECTED:
      return binance::json::OrderStatus::REJECTED;
  }
  return {};
}

static_assert(Helper{roq::OrderStatus::UNDEFINED} == binance::json::OrderStatus{binance::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::SENT} == binance::json::OrderStatus{binance::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::ACCEPTED} == binance::json::OrderStatus{binance::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::SUSPENDED} == binance::json::OrderStatus{binance::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::WORKING} == binance::json::OrderStatus{binance::json::OrderStatus::NEW});
static_assert(Helper{roq::OrderStatus::STOPPED} == binance::json::OrderStatus{binance::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::COMPLETED} == binance::json::OrderStatus{binance::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::EXPIRED} == binance::json::OrderStatus{binance::json::OrderStatus::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderStatus::CANCELED} == binance::json::OrderStatus{binance::json::OrderStatus::CANCELED});
static_assert(Helper{roq::OrderStatus::REJECTED} == binance::json::OrderStatus{binance::json::OrderStatus::REJECTED});

template <>
template <>
std::optional<binance::json::OrderStatus> Map<roq::OrderStatus>::helper() const {
  return Helper{args_};
}

// roq::OrderType ==> binance::json::OrderType

template <>
template <>
constexpr Helper<roq::OrderType>::operator std::optional<binance::json::OrderType>() const {
  switch (std::get<0>(args_)) {
    using enum roq::OrderType;
    case UNDEFINED:
      return binance::json::OrderType::UNDEFINED_INTERNAL;
    case MARKET:
      return binance::json::OrderType::MARKET;
    case LIMIT:
      return binance::json::OrderType::LIMIT;
  }
  return {};
}

static_assert(Helper{roq::OrderType::UNDEFINED} == binance::json::OrderType{binance::json::OrderType::UNDEFINED_INTERNAL});
static_assert(Helper{roq::OrderType::MARKET} == binance::json::OrderType{binance::json::OrderType::MARKET});
static_assert(Helper{roq::OrderType::LIMIT} == binance::json::OrderType{binance::json::OrderType::LIMIT});

template <>
template <>
std::optional<binance::json::OrderType> Map<roq::OrderType>::helper() const {
  return Helper{args_};
}

// roq::Side ==> binance::json::Side

template <>
template <>
constexpr Helper<roq::Side>::operator std::optional<binance::json::Side>() const {
  switch (std::get<0>(args_)) {
    using enum roq::Side;
    case UNDEFINED:
      return binance::json::Side::UNDEFINED_INTERNAL;
    case BUY:
      return binance::json::Side::BUY;
    case SELL:
      return binance::json::Side::SELL;
  }
  return {};
}

static_assert(Helper{roq::Side::UNDEFINED} == binance::json::Side{binance::json::Side::UNDEFINED_INTERNAL});
static_assert(Helper{roq::Side::BUY} == binance::json::Side{binance::json::Side::BUY});
static_assert(Helper{roq::Side::SELL} == binance::json::Side{binance::json::Side::SELL});

template <>
template <>
std::optional<binance::json::Side> Map<roq::Side>::helper() const {
  return Helper{args_};
}

// roq::TimeInForce ==> binance::json::TimeInForce

template <>
template <>
constexpr Helper<roq::TimeInForce>::operator std::optional<binance::json::TimeInForce>() const {
  switch (std::get<0>(args_)) {
    using enum roq::TimeInForce;
    case UNDEFINED:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
    case GFD:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
    case GTC:
      return binance::json::TimeInForce::GTC;
    case OPG:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
    case IOC:
      return binance::json::TimeInForce::IOC;
    case FOK:
      return binance::json::TimeInForce::FOK;
    case GTX:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
    case GTD:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
    case AT_THE_CLOSE:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
    case GOOD_THROUGH_CROSSING:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
    case AT_CROSSING:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
    case GOOD_FOR_TIME:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
    case GFA:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
    case GFM:
      return binance::json::TimeInForce::UNDEFINED_INTERNAL;
  }
  return {};
}

static_assert(Helper{roq::TimeInForce::UNDEFINED} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GFD} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GTC} == binance::json::TimeInForce{binance::json::TimeInForce::GTC});
static_assert(Helper{roq::TimeInForce::OPG} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::IOC} == binance::json::TimeInForce{binance::json::TimeInForce::IOC});
static_assert(Helper{roq::TimeInForce::FOK} == binance::json::TimeInForce{binance::json::TimeInForce::FOK});
static_assert(Helper{roq::TimeInForce::GTX} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GTD} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::AT_THE_CLOSE} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GOOD_THROUGH_CROSSING} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::AT_CROSSING} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GOOD_FOR_TIME} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GFA} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});
static_assert(Helper{roq::TimeInForce::GFM} == binance::json::TimeInForce{binance::json::TimeInForce::UNDEFINED_INTERNAL});

template <>
template <>
std::optional<binance::json::TimeInForce> Map<roq::TimeInForce>::helper() const {
  return Helper{args_};
}

}  // namespace roq
