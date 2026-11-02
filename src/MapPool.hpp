// Map Pool(방이 허용하는 맵 조건) 설정의 검증과 맵 적합성 판정, 조건 요약 문구 생성.
// 설정은 길이 마스크, 난이도 마스크, 최소 좋아요, Rated 여부, 2-Player 허용 방식으로 이루어진다.
#pragma once
#include "RandomMapPolicy.hpp"
#include "VersusService.hpp"

#include <optional>
#include <string>
#include <vector>

namespace versus {
// 설정 값들이 허용 범위 안에 있는지 검사한다.
inline bool validPoolSettings(MapPoolSettings const& settings) {
    return settings.perPlayer >= 1 && settings.perPlayer <= kPoolPerPlayerMax && settings.mask >= 1 &&
           settings.mask <= kPoolMaskAll && settings.twoPlayer >= 0 && settings.twoPlayer <= 2 &&
           settings.lengths >= 1 && settings.lengths <= kLengthsAll && settings.minLikes >= 0 &&
           settings.minLikes <= kLikesMax;
}
// 레벨이 풀 조건에 맞는지. Platformer와 길이 5는 항상 제외하고, filter가 꺼져 있으면 모두 통과.
// 난이도 마스크는 데몬 5종(비트 8~12), 2~9별(비트 0~7), 별이 없는 얼굴 난이도 순으로 확인한다.
// twoPlayer 설정: 0 상관없음, 1 2-Player 제외, 2 2-Player만.
inline bool poolLevelFits(LevelInfo const& level, bool twoPlayer, MapPoolSettings const& settings) {
    if (level.platformer || level.length == 5) return false;
    if (!settings.filter) return true;
    if (!lengthFits(level.length, settings.lengths)) return false;
    if (settings.rated && level.stars < 1) return false;
    if (settings.mask != kPoolMaskAll) {
        constexpr int demons[] = {7, 8, 6, 9, 10};
        constexpr int faces[] = {0, 1, 2, 12, 48, 192};
        bool fits = false;
        if (level.demon) {
            for (int i = 0; i < 5; ++i)
                if (level.difficulty == demons[i]) fits = (settings.mask & (1 << (8 + i))) != 0;
        } else if (level.stars >= 2 && level.stars <= 9) {
            fits = (settings.mask & (1 << (level.stars - 2))) != 0;
        } else if (level.stars == 0 && !level.autoLevel && level.difficulty >= 1 && level.difficulty <= 5) {
            fits = (settings.mask & faces[level.difficulty]) != 0;
        }
        if (!fits) return false;
    }
    if (settings.twoPlayer == 1 && twoPlayer) return false;
    return settings.twoPlayer != 2 || twoPlayer;
}
// 길이 마스크를 "Tiny / Short"처럼 표시한다. 전체면 빈 문자열.
inline std::string lengthsText(int lengths) {
    if (lengths == kLengthsAll) return {};
    constexpr char const* names[] = {"Tiny", "Short", "Medium", "Long", "XL"};
    std::string text;
    for (int length = 0; length <= 4; ++length)
        if (lengths & (1 << length)) text += (text.empty() ? "" : " / ") + std::string(names[length]);
    return text;
}
// 최소 좋아요 문구("5K+ likes" 등). 조건이 없으면 빈 문자열.
inline std::string likesText(int likes) {
    if (likes <= 0) return {};
    if (likes >= 1000 && likes % 1000 == 0) return std::to_string(likes / 1000) + "K+ likes";
    return std::to_string(likes) + "+ likes";
}
// 순 좋아요가 최소 조건을 만족하는지.
inline bool poolLikesFit(int netLikes, MapPoolSettings const& settings) {
    return !settings.filter || settings.minLikes <= 0 || netLikes >= settings.minLikes;
}
// 활성화된 조건들을 짧은 문구 목록으로 만든다. 난이도는 카테고리별로 쓰고,
// 일부 별만 켜진 카테고리는 별 번호를 괄호에 적으며, 5개 이상이면 개수로 줄인다.
inline std::vector<std::string> poolConditionParts(MapPoolSettings const& settings) {
    std::vector<std::string> parts;
    if (!settings.filter) return parts;
    if (auto const lengths = lengthsText(settings.lengths); !lengths.empty()) parts.push_back(lengths);
    if (auto const likes = likesText(settings.minLikes); !likes.empty()) parts.push_back(likes);
    if (settings.rated) parts.push_back("Rated only");
    if (settings.mask != kPoolMaskAll) {
        std::string difficulties;
        int count = 0;
        for (int i = 0; i < 10; ++i) {
            int const bits = settings.mask & CATEGORY_MASKS[i];
            if (!bits) continue;
            ++count;
            std::string part = RANDOM_DIFFICULTIES[i];
            if (bits != CATEGORY_MASKS[i]) {
                part += " (";
                bool first = true;
                for (int bit = 0; bit < 8; ++bit)
                    if (bits & (1 << bit)) {
                        part += (first ? "" : "/") + std::to_string(bit + 2);
                        first = false;
                    }
                part += "*)";
            }
            difficulties += (difficulties.empty() ? "" : ", ") + part;
        }
        parts.push_back(count > 4 ? std::to_string(count) + " difficulties" : difficulties);
    }
    if (settings.twoPlayer == 1) parts.push_back("No 2-Player");
    if (settings.twoPlayer == 2) parts.push_back("2-Player only");
    return parts;
}
// 조건을 한 줄로 합친다. 조건이 없으면 "Any Classic map".
inline std::string poolConditionText(MapPoolSettings const& settings) {
    auto const parts = poolConditionParts(settings);
    if (parts.empty()) return "Any Classic map";
    std::string text;
    for (auto const& part : parts) text += (text.empty() ? "" : "  |  ") + part;
    return text;
}
// 맵이 풀에 맞지 않을 때의 사유 문구. 맞으면 빈 문자열. netLikes를 알 때만 좋아요를 검사한다.
inline std::string poolMisfit(LevelInfo const& level, bool twoPlayer, MapPoolSettings const& settings,
                              std::optional<int> netLikes = std::nullopt) {
    if (level.platformer || level.length == 5) return "Versus supports Classic maps only.";
    if (!settings.filter) return {};
    if (!lengthFits(level.length, settings.lengths)) return "This map's length is not one the Map Pool takes.";
    if (netLikes && !poolLikesFit(*netLikes, settings))
        return "The Map Pool takes maps with " + likesText(settings.minLikes) + " only.";
    if (settings.rated && level.stars < 1) return "The Map Pool takes rated (star) maps only.";
    if (settings.twoPlayer == 1 && twoPlayer) return "The Map Pool takes no 2-Player maps.";
    if (settings.twoPlayer == 2 && !twoPlayer) return "The Map Pool takes 2-Player maps only.";
    if (!poolLevelFits(level, twoPlayer, settings)) return "This map's difficulty is not in the Map Pool's.";
    return {};
}
}
