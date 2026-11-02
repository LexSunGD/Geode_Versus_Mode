// 닉네임 색/글꼴, 칭호, 오라, 피니셔(execution) 등 꾸미기 선택값과 검증 규칙.
// 전부 순수 함수/상수이며, 선택값이 유효한지, 해당 에디션에서 쓸 수 있는지,
// 쓸 수 없으면 어떤 값으로 되돌릴지를 정한다.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace versus {
// 한 유저의 꾸미기 선택. start/end는 닉네임 그라데이션의 RGB(0xRRGGBB), angle은 도(0-359),
// aura/execution/title/font는 각 목록의 인덱스(또는 칭호 ID)이다.
struct Cosmetics {
    int start = 0x46FF64;
    int end = 0xFFFFFF;
    int angle = 0;
    int aura = 0;
    int execution = 0;
    int title = 0;
    int font = 0;
    bool operator==(Cosmetics const&) const = default;
};
// 닉네임 글꼴 개수.
inline constexpr int kNicknameFontCount = 60;
constexpr bool validNicknameFont(int font) {
    return font >= 0 && font < kNicknameFontCount;
}
// 피니셔(execution) 이름 목록. 0번은 무작위.
inline constexpr int kExecutionCount = 10;
inline constexpr std::array<char const*, kExecutionCount> kExecutionNames{
    "Random",         "Spike Volley",     "Fireball",      "Railgun",       "Blade Storm",
    "Arcane Verdict", "Phoenix Judgment", "Eclipse Slash", "Monster Chomp", "Sunset Showdown"};
// 기본 제공(비멤버십) 피니셔: 0(무작위), 1, 2, 8.
constexpr bool isStandardExecution(int choice) {
    return (choice >= 0 && choice <= 2) || choice == 8;
}
// choice가 범위 안이고, 기본 제공이거나 멤버십일 때 쓸 수 있다.
constexpr bool canUseExecution(int choice, bool membership) {
    return choice >= 0 && choice < kExecutionCount && (isStandardExecution(choice) || membership);
}
// 선택 화면에 보이는 피니셔 순서.
inline constexpr std::array<int, kExecutionCount> kExecutionDisplayOrder{0, 1, 2, 8, 3, 4, 5, 6, 7, 9};
// 무작위(0) 또는 사용 불가한 선택이면 seed 문자열의 FNV-1a 해시로 사용 가능한 피니셔 중
// 하나를 고정적으로 고른다. 같은 seed는 항상 같은 결과.
inline int resolveExecution(int choice, bool membership, std::string_view seed) {
    if (choice != 0 && canUseExecution(choice, membership)) return choice;
    std::uint32_t hash = 2166136261u;
    for (unsigned char c : seed) hash = (hash ^ c) * 16777619u;
    static constexpr std::array<int, 3> standard{1, 2, 8};
    static constexpr std::array<int, 9> member{1, 2, 3, 4, 5, 6, 7, 8, 9};
    return membership ? member[hash % member.size()] : standard[hash % standard.size()];
}
// 칭호 개수. 시즌 1 최종 칭호는 별도 ID 범위를 쓴다.
inline constexpr int kTitleCount = 99;
// 시즌 1 최종 칭호 ID 인코딩: base + tier * 10000 + rating.
inline constexpr int kS1FinalTitleBase = 1'000'000;
// 시즌 1 최종 칭호 ID 범위(티어 1~10)인지 확인한다.
constexpr bool isS1FinalTitle(int title) {
    return title >= kS1FinalTitleBase + 10'000 && title < kS1FinalTitleBase + 110'000;
}
// 시즌 1 최종 칭호 ID에서 티어와 레이팅을 꺼낸다. 해당 ID가 아니면 0.
constexpr int s1FinalTier(int title) {
    return isS1FinalTitle(title) ? (title - kS1FinalTitleBase) / 10'000 : 0;
}
constexpr int s1FinalRating(int title) {
    return isS1FinalTitle(title) ? (title - kS1FinalTitleBase) % 10'000 : 0;
}
// 일반 칭호 범위이거나 시즌 1 최종 칭호이면 유효하다.
constexpr bool validTitle(int title) {
    return (title >= 0 && title < kTitleCount) || isS1FinalTitle(title);
}
static_assert(s1FinalTier(1'102'126) == 10 && s1FinalRating(1'102'126) == 2126 && !isS1FinalTitle(1'000'500));
// 오라 개수와 이름. 8번은 "None".
inline constexpr int kAuraCount = 18;
inline constexpr std::array<char const*, kAuraCount> kAuraNames{
    "Aurora", "Inferno", "Arcane",  "Frost",   "Storm",  "Solar", "Void", "Starlight", "None",
    "Seraph", "Lunar",   "Phoenix", "Crystal", "Astral", "Fairy", "Wisp", "Excalibur", "Wyvern"};
// 오라 목록 필터 카테고리.
enum class AuraCategory { All, Standard, Membership };
inline constexpr std::array<char const*, 3> kAuraCategoryNames{"All", "Standard", "Membership"};
// 선택 화면에 보이는 오라 순서.
inline constexpr std::array<int, kAuraCount> kAuraDisplayOrder{8,  0,  1, 2,  3,  4,  5,  6,  7,
                                                               14, 15, 9, 10, 11, 12, 13, 16, 17};
// 기본 제공(비멤버십) 오라: 0-8번 중 2번 제외, 14, 15.
constexpr bool isStandardAura(int aura) {
    return (aura >= 0 && aura <= 8 && aura != 2) || aura == 14 || aura == 15;
}
constexpr bool canUseAura(int aura, bool membership) {
    return aura >= 0 && aura < kAuraCount && (membership || isStandardAura(aura));
}
// 카테고리 필터에 해당하는 오라인지 확인한다. None(8)은 모든 카테고리에 보인다.
constexpr bool auraInCategory(int aura, AuraCategory category) {
    if (aura < 0 || aura >= kAuraCount) return false;
    return aura == 8 || category == AuraCategory::All ||
           (category == AuraCategory::Standard ? isStandardAura(aura) : !isStandardAura(aura));
}
// 카테고리 필터에 해당하는 피니셔인지 확인한다.
constexpr bool executionInCategory(int choice, AuraCategory category) {
    if (choice < 0 || choice >= kExecutionCount) return false;
    return category == AuraCategory::All ||
           (category == AuraCategory::Standard ? isStandardExecution(choice) : !isStandardExecution(choice));
}
// 에디션별 기본 꾸미기. 비멤버십은 오라 None으로 시작한다.
constexpr Cosmetics defaultCosmetics(bool membership) {
    Cosmetics result;
    if (!membership) result.aura = 8;
    return result;
}
// 에디션별 로컬 저장 키.
constexpr char const* cosmeticsSaveKey(bool membership) {
    return membership ? "cosmetics-v1" : "cosmetics-standard-v1";
}
// 모든 필드가 허용 범위 안인지 확인한다.
constexpr bool validCosmetics(Cosmetics const& value) {
    return value.start >= 0 && value.start <= 0xFFFFFF && value.end >= 0 && value.end <= 0xFFFFFF && value.angle >= 0 &&
           value.angle < 360 && value.aura >= 0 && value.aura < kAuraCount && value.execution >= 0 &&
           value.execution < kExecutionCount && validTitle(value.title) && validNicknameFont(value.font);
}
// 유효하지 않으면 통째로 기본값으로 되돌린다.
constexpr Cosmetics sanitizeCosmetics(Cosmetics value) {
    return validCosmetics(value) ? value : Cosmetics{};
}
// 해당 에디션에서 선택 전체를 쓸 수 있는지 확인한다. 비멤버십은 색 설정(그라데이션)도
// 기본값이어야 한다.
constexpr bool canUseCosmetics(Cosmetics const& value, bool membership) {
    return validCosmetics(value) && canUseAura(value.aura, membership) &&
           canUseExecution(value.execution, membership) &&
           (membership || (value.start == Cosmetics{}.start && value.end == Cosmetics{}.end && value.angle == 0));
}
// 에디션에 맞게 선택값을 정리한다. 비멤버십은 사용 가능한 항목(오라, 피니셔, 칭호, 글꼴)만
// 남기고 나머지는 기본값으로 되돌린다.
constexpr Cosmetics cosmeticsForEdition(Cosmetics value, bool membership) {
    if (membership) return sanitizeCosmetics(value);
    auto result = defaultCosmetics(false);
    if (isStandardAura(value.aura)) result.aura = value.aura;
    if (canUseExecution(value.execution, false)) result.execution = value.execution;
    if (validTitle(value.title)) result.title = value.title;
    if (validNicknameFont(value.font)) result.font = value.font;
    return result;
}
// start에서 end로 가는 그라데이션에서 position(0~1)의 RGB 색을 계산한다.
constexpr std::array<unsigned char, 3> gradientColor(float position, Cosmetics value) {
    value = sanitizeCosmetics(value);
    auto const t = std::clamp(position, 0.f, 1.f);
    std::array<unsigned char, 3> result{};
    for (int channel = 0; channel < 3; ++channel) {
        int const shift = 16 - channel * 8;
        int const first = (value.start >> shift) & 255, last = (value.end >> shift) & 255;
        result[channel] = static_cast<unsigned char>(first + (last - first) * t);
    }
    return result;
}
// 각도(도)를 그라데이션 방향 벡터로 바꾼다. 범위를 벗어난 각도는 0으로 본다.
inline std::array<float, 2> gradientAxis(int angle) {
    float const radians = (angle >= 0 && angle < 360 ? angle : 0) * .01745329252f;
    return {std::cos(radians), std::sin(radians)};
}
// 사각형(width x height) 안의 점 (x, y)가 방향 axis 기준으로 그라데이션 어디쯤인지 0~1로
// 계산한다.
inline float gradientPosition(float x, float y, float width, float height, std::array<float, 2> axis) {
    float const span = std::abs(axis[0]) * width + std::abs(axis[1]) * height;
    return span > .0001f
               ? std::clamp(.5f + ((x - width * .5f) * axis[0] + (y - height * .5f) * axis[1]) / span, 0.f, 1.f)
               : 0.f;
}
}
