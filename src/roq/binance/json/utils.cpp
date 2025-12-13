/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/binance/json/utils.hpp"

#include "roq/logging.hpp"

#include "roq/binance/json/map.hpp"

using namespace std::literals;

namespace roq {
namespace binance {
namespace json {

// NOLINTBEGIN(readability-magic-numbers)

Error guess_error(int32_t code) {
  switch (code) {
      // https://binance-docs.github.io/apidocs/spot/en/#10xx-general-server-or-network-issues
    case -1000:  // UNKNOWN
      break;
    case -1001:  // DISCONNECTED
      break;
    case -1002:  // UNAUTHORIZED
      return Error::NOT_AUTHORIZED;
    case -1003:  // TOO_MANY_REQUESTS
      return Error::REQUEST_RATE_LIMIT_REACHED;
    case -1004:  // SERVER_BUSY
      break;
    case -1006:  // UNEXPECTED_RESP
      break;
    case -1007:  // TIMEOUT
      break;
    case -1014:  // UNKNOWN_ORDER_COMPOSITION
      break;
    case -1015:  // TOO_MANY_ORDERS
      return Error::REQUEST_RATE_LIMIT_REACHED;
    case -1016:  // SERVICE_SHUTTING_DOWN
      break;
    case -1020:  // UNSUPPORTED_OPERATION
      break;
    case -1021:  // INVALID_TIMESTAMP
      return Error::INVALID_REQUEST_ARGS;
    case -1022:  // INVALID_SIGNATURE
      return Error::NOT_AUTHORIZED;
    case -1099:  //  Not found, authenticated, or authorized
      return Error::NOT_AUTHORIZED;
    // https://binance-docs.github.io/apidocs/spot/en/#11xx-2xxx-request-issues
    case -1100:  // ILLEGAL_CHARS
      return Error::INVALID_REQUEST_ARGS;
    case -1101:  // TOO_MANY_PARAMETERS
      return Error::INVALID_REQUEST_ARGS;
    case -1102:  // MANDATORY_PARAM_EMPTY_OR_MALFORMED
      return Error::INVALID_REQUEST_ARGS;
    case -1103:  // UNKNOWN_PARAM
      return Error::INVALID_REQUEST_ARGS;
    case -1104:  // UNREAD_PARAMETERS
      return Error::INVALID_REQUEST_ARGS;
    case -1105:  // PARAM_EMPTY
      return Error::INVALID_REQUEST_ARGS;
    case -1106:  // PARAM_NOT_REQUIRED
      return Error::INVALID_REQUEST_ARGS;
    case -1111:  // BAD_PRECISION
      return Error::INVALID_REQUEST_ARGS;
    case -1112:  // NO_DEPTH
      break;
    case -1114:  // TIF_NOT_REQUIRED
      return Error::INVALID_TIME_IN_FORCE;
    case -1115:  // INVALID_TIF
      return Error::INVALID_TIME_IN_FORCE;
    case -1116:  // INVALID_ORDER_TYPE
      return Error::INVALID_ORDER_TYPE;
    case -1117:  // INVALID_SIDE
      return Error::INVALID_SIDE;
    case -1118:  // EMPTY_NEW_CL_ORD_ID
      return Error::INVALID_REQUEST_ID;
    case -1119:  // EMPTY_ORG_CL_ORD_ID
      return Error::INVALID_REQUEST_ID;
    case -1120:  // BAD_INTERVAL
      break;
    case -1121:  // BAD_SYMBOL
      return Error::INVALID_SYMBOL;
    case -1125:  // INVALID_LISTEN_KEY
      return Error::NOT_AUTHORIZED;
    case -1127:  // MORE_THAN_XX_HOURS
      break;
    case -1128:  // OPTIONAL_PARAMS_BAD_COMBO
      return Error::INVALID_REQUEST_ARGS;
    case -1130:  // INVALID_PARAMETER
      return Error::INVALID_REQUEST_ARGS;
    case -1131:  // BAD_RECV_WINDOW
      return Error::INVALID_REQUEST_ARGS;
    case -2008:
      return Error::NOT_AUTHORIZED;
    case -2010:  // NEW_ORDER_REJECTED
      break;
    case -2011:  // CANCEL_REJECTED
      return Error::TOO_LATE_TO_MODIFY_OR_CANCEL;
    case -2013:  // NO_SUCH_ORDER
      return Error::UNKNOWN_ORDER_ID;
    case -2014:  // BAD_API_KEY_FMT
      break;
    case -2015:  // REJECTED_MBX_KEY
      break;
    case -2016:  // NO_TRADING_WINDOW
      break;
      // https://binance-docs.github.io/apidocs/spot/en/#3xxx-5xxx-sapi-specific-issues
      // https://binance-docs.github.io/apidocs/spot/en/#6xxx-savings-issues
      // ...
  }
  log::warn<2>("DEBUG Unable to map error code={}"sv, code);
  return Error::UNKNOWN;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
