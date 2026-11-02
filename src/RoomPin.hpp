// 방 비밀번호(PIN) 검증. PIN은 숫자만, 1~MAX_ROOM_PIN자.
#pragma once
#include <algorithm>
#include <string_view>

namespace versus {
// PIN 최대 길이.
inline constexpr size_t MAX_ROOM_PIN = 32;
constexpr bool validRoomPin(std::string_view pin) {
    return !pin.empty() && pin.size() <= MAX_ROOM_PIN &&
           std::all_of(pin.begin(), pin.end(), [](char c) { return c >= '0' && c <= '9'; });
}
}
