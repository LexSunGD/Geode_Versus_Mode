// 안티치트 감지 코드(문자열)와 사유 문구 헬퍼.
// 코드는 감지 출처와 종류(enabled = 설정이 켜져 있음, attempt = 시도 흔적)를 나타내며
// 서버로 전송되고 결과 화면의 사유 문구를 만드는 데 쓰인다.
#pragma once

#include <string>
#include <string_view>

namespace versus::integrity {

inline constexpr std::string_view kEclipseEnabled = "eclipse:enabled";
inline constexpr std::string_view kEclipseAttempt = "eclipse:attempt";
inline constexpr std::string_view kQolEnabled = "qol:enabled";
inline constexpr std::string_view kMegaHackEnabled = "megahack:enabled";
inline constexpr std::string_view kMegaHackAttempt = "megahack:attempt";
inline constexpr std::string_view kPrismEnabled = "prism:enabled";
inline constexpr std::string_view kBannedModEnabled = "mod:enabled";

// 빈 코드(감지 없음)이거나 알려진 코드 중 하나인지 확인한다.
inline bool validCode(std::string_view code) {
    return code.empty() || code == kEclipseEnabled || code == kEclipseAttempt || code == kQolEnabled ||
           code == kMegaHackEnabled || code == kMegaHackAttempt || code == kPrismEnabled || code == kBannedModEnabled;
}

// 다른 모드에서 온 감지 코드인지 확인한다. 빈 코드는 제외.
inline bool externalCode(std::string_view code) {
    return code == kEclipseEnabled || code == kEclipseAttempt || code == kQolEnabled || code == kMegaHackEnabled ||
           code == kMegaHackAttempt || code == kPrismEnabled || code == kBannedModEnabled;
}

// 코드에 해당하는 감지 출처 이름. 알 수 없는 코드는 Versus 자체로 본다.
inline std::string sourceName(std::string_view code) {
    if (code == kEclipseEnabled || code == kEclipseAttempt) return "Eclipse";
    if (code == kQolEnabled) return "QOLMod";
    if (code == kMegaHackEnabled || code == kMegaHackAttempt) return "Mega Hack";
    if (code == kPrismEnabled) return "Prism Menu";
    if (code == kBannedModEnabled) return "Unauthorized mod";
    return "Versus";
}

// 결과 화면에 표시할 사유. 유효하지 않거나 빈 코드는 일반 기권 문구로 처리한다.
// opponent가 true면 상대 기준 문구.
inline std::string resultReason(std::string_view code, bool opponent) {
    if (!validCode(code) || code.empty()) return opponent ? "Opponent forfeited" : "Match forfeited";
    return opponent ? "Opponent disqualified - " + sourceName(code) + " anti-cheat"
                    : "Disqualified - " + sourceName(code) + " anti-cheat";
}

}
