/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/binance/json/ws_api_request.hpp"

#include "roq/utils/codec/base64.hpp"

using namespace std::literals;

using namespace fmt::literals;

namespace roq {
namespace binance {
namespace json {

// === IMPLEMENTATION ===

std::string_view WSAPIRequest::encode(std::vector<char> &buffer, WSAPIRequest const &request) {
  std::array<std::byte, 26> data;
  std::memcpy(&data[0], &request.sequence, 4);
  auto event_type = static_cast<uint8_t>(static_cast<json::WSAPIType::type_t>(request.type));
  data[4] = *reinterpret_cast<std::byte const *>(&event_type);
  data[5] = *reinterpret_cast<std::byte const *>(&request.user_id);
  std::memcpy(&data[6], &request.order_id, 8);
  std::memcpy(&data[14], &request.version, 4);
  std::memcpy(&data[18], &request.order_id_2, 8);
  auto result = utils::codec::Base64::encode(buffer, data, true, false);
  return {std::data(result), std::size(result)};
}

WSAPIRequest WSAPIRequest::decode(std::string_view const &buffer) {
  if (std::size(buffer) != 36)
    throw RuntimeError{"Unexpected: len(buffer)={}"sv, std::size(buffer)};
  std::array<std::byte, 27> data;  // note! need 1 extra
  utils::codec::Base64::decode(data, std::span{std::data(buffer), std::size(buffer)}, true, false);
  WSAPIRequest result;
  std::memcpy(&result.sequence, &data[0], 4);
  result.type = json::WSAPIType{static_cast<json::WSAPIType::type_t>(std::to_integer<uint8_t>(data[4]))};
  result.user_id = std::to_integer<uint8_t>(data[5]);
  std::memcpy(&result.order_id, &data[6], 8);
  std::memcpy(&result.version, &data[14], 4);
  std::memcpy(&result.order_id_2, &data[18], 8);
  return result;
}

}  // namespace json
}  // namespace binance
}  // namespace roq
