/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/binance/json/parser.h"

#include "roq/compat.h"

#include "roq/binance/json/field.h"
#include "roq/binance/json/stream.h"

#include "roq/logging.h"

namespace roq {
namespace binance {
namespace json {

void Parser::dispatch(
    Parser::Handler& handler,
    const std::string_view& message,
    core::json::Buffer& buffer) {
  std::string_view symbol;
  auto stream = Stream::UNDEFINED;
  for (int i = 0; i < 2; ++i) {
    core::json::Parser parser(message);
    auto root = parser.root();
    for (auto [key, value] : std::get<core::json::object_t>(root)) {
      auto field = Field(key);
      switch (field) {
        case Field::UNDEFINED:
          LOG(FATAL)("Unexpected");
          break;
        case Field::UNKNOWN:
          DLOG(FATAL)(
              FMT_STRING(R"(Unknown key="{}")"),
              key);
          // XXX CALLBACK ?????????????
          break;
        case Field::ERROR: {
          Error error(value);
          LOG(INFO)(FMT_STRING("ERROR error={}"), error);
          // XXX CALLBACK ?????????????
          return;
        }
        case Field::RESULT: {
          Result result(
              value,
              buffer);
          LOG(INFO)(FMT_STRING("RESULT result={}"), result);
          // XXX CALLBACK ?????????????
          return;
        }
        case Field::ID:
          // wait for error or result
          break;
        case Field::STREAM: {
          // <symbol>@<stream>[@<freq>]
          auto full_name = std::get<std::string_view>(value);
          auto idx0 = full_name.find('@');  // <symbol>@<stream>
          LOG_IF(FATAL, idx0 == full_name.npos)(
              FMT_STRING(R"(Unexpected: name="")"),
              full_name);
          symbol = std::string_view(
              full_name.begin(),
              idx0);
          auto idx1 = full_name.find('@', idx0 + 1);
          auto name = std::string_view(
              full_name.begin() + idx0 + 1,
              (idx1 == full_name.npos)
                ? full_name.size() - idx0 - 1
                : idx1 - idx0 - 1);
          stream = Stream(name);
          break;
        }
        case Field::DATA:
          switch (stream) {
            case Stream::UNDEFINED:
              break;  // wait
            case Stream::UNKNOWN:
              DLOG(FATAL)("Unexpected (unknown stream)");
              return;
            case Stream::TRADE: {
              Trade trade(value);
              handler(trade);
              return;
            }
            case Stream::BOOK_TICKER: {
              BookTicker book_ticker(value);
              handler(book_ticker);
              return;
            }
            case Stream::DEPTH5:
            case Stream::DEPTH10:
            case Stream::DEPTH20: {
              assert(symbol.empty() == false);
              Depth depth(
                  value,
                  buffer);
              handler(
                  symbol,
                  depth);
              return;
            }
            case Stream::DEPTH: {
              assert(symbol.empty() == false);
              DepthUpdate depth_update(
                  value,
                  buffer);
              handler(
                  symbol,
                  depth_update);
              return;
            }
          }
          break;
      }
    }
  }
  LOG(WARNING)(
      FMT_STRING(R"(message="{}")"),
      message);
  LOG(FATAL)("Unexpected");
}

}  // namespace json
}  // namespace binance
}  // namespace roq
