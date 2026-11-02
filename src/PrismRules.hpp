// Prism Menu에서 금지하는 게임플레이 기능 목록과 판정 규칙.
// id는 Prism 저장 값의 이름이고 name은 화면 표시 이름이다.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace versus::integrity {

// Prism 기능 하나: 저장 값 이름과 표시 이름.
struct PrismCheat {
    std::string_view id;
    std::string_view name;
};

// 대전에서 금지 대상인 Prism 기능들.
inline constexpr std::array<PrismCheat, 22> kPrismGameplayCheats{{
    {"Noclip", "Noclip"},
    {"Instant Complete", "Instant Complete"},
    {"No Spikes", "No Spikes"},
    {"No Hitbox", "No Hitbox"},
    {"No Solids", "No Solids"},
    {"No Rotate", "No Rotate"},
    {"Hide Player", "Hide Player"},
    {"Freeze Player", "Freeze Player"},
    {"Jump Hack", "Jump Hack"},
    {"Layout Mode", "Layout Mode"},
    {"Show Hidden Objects", "Show Hidden Objects"},
    {"Hide Level", "Hide Level"},
    {"Change Gravity", "Change Gravity"},
    {"Force Platformer Mode", "Force Platformer Mode"},
    {"No Mirror Transition", "No Mirror Transition"},
    {"Instant Mirror Portal", "Instant Mirror Portal"},
    {"Show Hitboxes", "Show Hitboxes"},
    {"No Shaders", "No Shaders"},
    {"Playback", "Quartz Playback"},
    {"Disable Camera Effects", "Disable Camera Effects"},
    {"Auto Clicker", "Auto Clicker"},
    {"Speedhack", "Speedhack"},
}};

// id에 해당하는 기능 정보. 없으면 nullptr.
inline PrismCheat const* prismGameplayCheat(std::string_view id) {
    for (auto const& cheat : kPrismGameplayCheats)
        if (cheat.id == id) return &cheat;
    return nullptr;
}

// id에 해당하는 표시 이름. 없으면 빈 문자열.
inline std::string_view prismGameplayCheatName(std::string_view id) {
    if (auto const* cheat = prismGameplayCheat(id)) return cheat->name;
    return {};
}

// scheduler dt가 훅을 지나며 상대 오차 0.01%(최소 1e-7)보다 크게 바뀌었는지 확인한다.
// 값이 유한하지 않거나 0 이하이면 판정하지 않는다.
inline bool prismSchedulerDeltaIndicatesSpeed(double before, double after) {
    if (!std::isfinite(before) || !std::isfinite(after) || before <= 0. || after <= 0.) return false;
    return std::abs(after - before) > std::max(1e-7, std::abs(before) * .0001);
}

// 켜진 Prism 기능 ID 목록을 만든다. Speedhack은 토글이 아니라 배속 값이 1이 아닐 때만
// 포함한다.
template <typename ReadBool, typename ReadNumber>
inline std::vector<std::string> activePrismGameplayFeatures(ReadBool&& readBool, ReadNumber&& readNumber) {
    std::vector<std::string> detected;
    for (auto const& cheat : kPrismGameplayCheats) {
        if (cheat.id == "Speedhack") continue;
        if (readBool(cheat.id)) detected.emplace_back(cheat.id);
    }
    auto const speed = readNumber("Speedhack", 1.);
    if (std::isfinite(speed) && std::abs(speed - 1.) > .0001) detected.emplace_back("Speedhack");
    return detected;
}

}
