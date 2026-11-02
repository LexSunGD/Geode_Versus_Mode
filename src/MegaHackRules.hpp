// Mega Hack이 켜 둔 게임플레이 치트 목록과, 그 설정이 실제로 값을 바꾸고 있는지 판정하는
// 규칙.
// UI 요소 ID(문자열)로 활성 여부와 수치를 읽는 함수를 주입받으므로 Mega Hack 자체에는
// 의존하지 않는다.
#pragma once

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace versus::integrity {

// Mega Hack 기능 하나: 내부 UI ID와 화면에 보이는 이름.
struct MegaHackCheat {
    std::string_view id;
    std::string_view name;
};

// Respawn Time 기본값(초). 이보다 느리게 설정되면 치트로 본다.
inline constexpr double kDefaultRespawnSeconds = 1.;
// 부동소수 오차(1e-4)를 허용하고, NaN/inf는 제외하고 기본값보다 느린지 판단한다.
inline bool respawnSlowerThanDefault(double seconds) {
    return std::isfinite(seconds) && seconds > kDefaultRespawnSeconds + 1e-4;
}

// 대전에서 금지 대상인 Mega Hack 게임플레이 기능들. 단순히 켜져 있는 것만으로는 부족한
// 항목은 아래 함수에서 수치를 추가로 본다.
inline constexpr std::array<MegaHackCheat, 15> kMegaHackGameplayCheats{{
    {"LEVEL/AUTO_CLICKER/HACK_AUTO_CLICKER", "Auto Clicker"},
    {"LEVEL/FRAME_STEPPER/HACK_FRAME_STEPPER", "Frame Stepper"},
    {"LEVEL/RESPAWN_TIME/HACK_RESPAWN_TIME", "Slower Respawn Time"},
    {"LEVEL/HACK_FORCE_PLATFORMER", "Force Platformer"},
    {"LEVEL/HACK_INSTANT_COMPLETE", "Instant Complete"},
    {"LEVEL/HACK_JUMP_HACK", "Jump Hack"},
    {"LEVEL/HITBOX_MULTIPLIER/HACK_HITBOX_MULTIPLIER", "Hitbox Multiplier"},
    {"LEVEL/NOCLIP/HACK_NOCLIP", "Noclip"},
    {"LEVEL/NO_COLLISION/HACK_NO_COLLISION", "No Collision"},
    {"LEVEL/SHOW_HITBOXES/HACK_SHOW_HITBOXES", "Show Hitboxes"},
    {"LEVEL/SHOW_LAYOUT/HACK_SHOW_LAYOUT", "Show Layout"},
    {"LEVEL/SHOW_TRAJECTORY/HACK_SHOW_TRAJECTORY", "Show Trajectory"},
    {"REPLAY/PLAYBACK/HACK_REPLAY_PLAYBACK", "Replay Playback"},
    {"SPEEDHACK/HACK_SPEEDHACK_CLASSIC", "Classic Speedhack"},
    {"SPEEDHACK/LAYOUT/HACK_SPEEDHACK", "Speedhack"},
}};

// ID에 해당하는 치트 정보를 찾는다. 목록에 없으면 nullptr.
inline MegaHackCheat const* megaHackGameplayCheat(std::string_view id) {
    for (auto const& cheat : kMegaHackGameplayCheats)
        if (id == cheat.id) return &cheat;
    return nullptr;
}

// ID에 해당하는 치트의 표시 이름. 없으면 빈 문자열.
inline std::string_view megaHackGameplayCheatName(std::string_view id) {
    if (auto const* cheat = megaHackGameplayCheat(id)) return cheat->name;
    return {};
}

// 현재 실제로 효과가 있는 치트 ID 목록을 만든다.
// isEnabled(id)는 토글 상태, readDecimal(id, 기본값)은 숫자 입력값을 읽는 콜백이다.
template <class IsEnabled, class ReadDecimal>
inline std::vector<std::string> activeMegaHackGameplayFeatures(IsEnabled&& isEnabled, ReadDecimal&& readDecimal) {
    std::vector<std::string> detected;
    for (auto const& cheat : kMegaHackGameplayCheats) {
        if (!isEnabled(cheat.id)) continue;
        // Speedhack은 배속이 1이 아닐 때만 효과가 있는 것으로 본다.
        if (cheat.id == "SPEEDHACK/HACK_SPEEDHACK_CLASSIC" || cheat.id == "SPEEDHACK/LAYOUT/HACK_SPEEDHACK") {
            auto const speed = readDecimal("SPEEDHACK/LAYOUT/SPINNER", 1.);
            if (!std::isfinite(speed) || std::abs(speed - 1.) <= .0001) continue;
        }
        // Respawn Time은 값(ms)을 초로 바꿔 기본값보다 느릴 때만 해당된다.
        if (cheat.id == "LEVEL/RESPAWN_TIME/HACK_RESPAWN_TIME" &&
            !respawnSlowerThanDefault(readDecimal("LEVEL/RESPAWN_TIME/DROPDOWN/SPINNER", 1000.) / 1000.))
            continue;
        // Hitbox Multiplier는 Hazards/Saws/Solids 중 하나라도 1배가 아닐 때만 해당된다.
        if (cheat.id == "LEVEL/HITBOX_MULTIPLIER/HACK_HITBOX_MULTIPLIER") {
            auto const hazards = readDecimal("LEVEL/HITBOX_MULTIPLIER/DROPDOWN/HAZARDS/MULTIPLIER", 1.);
            auto const saws = readDecimal("LEVEL/HITBOX_MULTIPLIER/DROPDOWN/SAWS/MULTIPLIER", 1.);
            auto const solids = readDecimal("LEVEL/HITBOX_MULTIPLIER/DROPDOWN/SOLIDS/MULTIPLIER", 1.);
            if (!std::isfinite(hazards) || !std::isfinite(saws) || !std::isfinite(solids) ||
                (std::abs(hazards - 1.) <= .0001 && std::abs(saws - 1.) <= .0001 && std::abs(solids - 1.) <= .0001))
                continue;
        }
        detected.emplace_back(cheat.id);
    }
    return detected;
}

}
