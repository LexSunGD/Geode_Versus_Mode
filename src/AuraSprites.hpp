// 이미지 파일을 쓰는 오라(aura) 스프라이트의 사양 표. 오라 ID로 조회해 Edition.cpp에서
// 쓴다.
#pragma once
#include <algorithm>
#include <array>

namespace versus {
// 오라 ID, 이미지 파일명, box(긴 변을 맞출 목표 크기), period(초),
// rotate가 true면 period는 1회전 시간, false면 투명도 변화 한 단계의 시간이다.
struct AuraSpriteSpec {
    int id;
    char const* file;
    float box, period;
    bool rotate;
};
inline constexpr std::array<AuraSpriteSpec, 8> kAuraSprites{{{2, "aura-arcane.png", 64.f, 24.f, true},
                                                             {9, "aura-seraph.png", 70.f, 2.2f, false},
                                                             {10, "aura-lunar.png", 68.f, 2.6f, false},
                                                             {11, "aura-phoenix.png", 70.f, 1.5f, false},
                                                             {12, "aura-crystal.png", 64.f, 24.f, true},
                                                             {13, "aura-astral.png", 64.f, 32.f, true},
                                                             {16, "aura-excalibur.png", 68.f, 28.f, true},
                                                             {17, "aura-wyvern.png", 70.f, 1.8f, false}}};
// ID에 해당하는 사양을 찾는다. 이미지가 없는 오라는 nullptr.
constexpr AuraSpriteSpec const* auraSpriteSpec(int id) {
    for (auto const& spec : kAuraSprites)
        if (spec.id == id) return &spec;
    return nullptr;
}
// 이미지의 긴 변이 box에 맞도록 하는 배율. 크기가 유효하지 않으면 0.
constexpr float auraSpriteScale(AuraSpriteSpec const& spec, float width, float height) {
    return width > 0.f && height > 0.f ? spec.box / std::max(width, height) : 0.f;
}
}
