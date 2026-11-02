// 맵의 이전 플레이 기록(최고 퍼센트)을 짧은 문구와 색으로 바꾸는 헬퍼.
#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <string>
#include <utility>

namespace versus {
// best >= 100은 클리어, 0 이상은 최고 퍼센트, -1은 처음 플레이, 그 외는 빈 문자열.
// compact이면 좁은 UI용 짧은 문구를 쓴다.
inline std::pair<std::string, cocos2d::ccColor3B> mapRecordText(int best, bool compact = false) {
    if (best >= 100) return {compact ? "Cleared" : "Cleared this map before", cocos2d::ccc3(255, 214, 90)};
    if (best >= 0)
        return {compact ? fmt::format("Best {}%", best) : fmt::format("Played before: best {}%", best),
                cocos2d::ccc3(140, 255, 170)};
    if (best == -1) return {compact ? "New" : "First time on this map", cocos2d::ccc3(160, 180, 210)};
    return {"", cocos2d::ccc3(255, 255, 255)};
}
}
