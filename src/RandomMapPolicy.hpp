// 랜덤 맵 선택 규칙(필터 검증, 난이도 비트마스크, 후보 매칭, 룰렛 연출 타이밍) 모음.
// MapSelection.mask의 13비트: 0~7은 별 2~9, 8~12는 Easy Demon 등 5가지 Demon 난이도.
#pragma once
#include "VersusService.hpp"
#include <algorithm>
#include <cmath>
#include <bit>

namespace versus {
// 랜덤 선택 UI에 쓰는 난이도 이름(10개 카테고리).
inline constexpr char const* RANDOM_DIFFICULTIES[] = {"Easy",         "Normal",       "Hard",         "Harder",
                                                      "Insane",       "Easy Demon",   "Medium Demon", "Hard Demon",
                                                      "Insane Demon", "Extreme Demon"};
// 카테고리별로 mask에서 차지하는 비트들.
inline constexpr int CATEGORY_MASKS[] = {1, 2, 12, 48, 192, 256, 512, 1024, 2048, 4096};
// 카테고리별 난이도 얼굴 아이콘 번호.
inline constexpr int CATEGORY_FACES[] = {1, 2, 3, 4, 5, 7, 8, 6, 9, 10};
// 모든 선택지가 켜진 mask 값(13비트).
inline constexpr int ALL_MAP_CHOICES = 8191;
// 랜덤 후보로 필요한 최소 레벨 수.
inline constexpr size_t MIN_RANDOM_CANDIDATES = 5;
// 예전 난이도 인덱스(0~9)를 mask로 바꾼다. 범위 밖이면 0.
inline int legacySelectionMask(int difficulty) {
    return difficulty >= 0 && difficulty < 10 ? CATEGORY_MASKS[difficulty] : 0;
}
// 룰렛 연출 단계 길이(ms): 시작 대기, 회전, 끝난 뒤 여운. DRAW_END_MS는 전체 길이.
inline constexpr int64_t DRAW_LEAD_MS = 1400;
inline constexpr int64_t DRAW_SPIN_MS = 4550;
inline constexpr int64_t DRAW_END_MS = DRAW_LEAD_MS + DRAW_SPIN_MS + 840;
// MapSelection 값이 허용 범위 안인지 검사한다(Platformer 불가, 랜덤이면 mask가 비면 안 됨 등).
inline bool validSelection(MapSelection const& filter) {
    return !filter.platformer && filter.mask >= 0 && filter.mask <= ALL_MAP_CHOICES &&
           (!filter.random || filter.mask != 0) && filter.lengths >= 1 && filter.lengths <= kLengthsAll &&
           filter.twoPlayer >= 0 && filter.twoPlayer <= 2 && filter.minLikes >= 0 && filter.minLikes <= kLikesMax;
}
// 길이 값이 허용된 길이 비트마스크에 들어가는지. 길이를 모르는(<0) 레벨은 통과시킨다.
inline bool lengthFits(int length, int lengths) {
    return lengths == kLengthsAll || length < 0 || (length <= 4 && (lengths & (1 << length)) != 0);
}
// 난이도 얼굴 번호(1~5)를 해당 별 카테고리 mask로 바꾼다.
inline int faceCategoryMask(int face) {
    constexpr int masks[] = {0, 1, 2, 12, 48, 192};
    return face >= 1 && face <= 5 ? masks[face] : 0;
}
// 레벨이 랜덤 맵 필터에 맞는지 검사한다. 이름/ID, 오토/Platformer/길이 조건을 보고,
// 별 0은 비평가 맵만, 별 2~9는 일반 난이도 비트, 별 10 Demon은 난이도 얼굴로 Demon 비트를 확인한다.
inline bool matchesRandomMap(LevelInfo const& level, MapSelection const& filter) {
    if (!validSelection(filter) || level.id <= 0 || level.name.empty() || level.name.size() > 64 || level.autoLevel ||
        level.platformer || level.length == 5 || !lengthFits(level.length, filter.lengths))
        return false;
    if (level.stars == 0)
        return !filter.rated && !level.demon && (filter.mask & faceCategoryMask(level.difficulty)) != 0;
    if (level.stars < 0) return false;
    if (!level.demon) {
        return level.stars >= 2 && level.stars <= 9 && (filter.mask & (1 << (level.stars - 2))) != 0;
    }
    constexpr int demons[] = {7, 8, 6, 9, 10};
    for (int i = 0; i < 5; ++i)
        if (level.stars == 10 && level.difficulty == demons[i]) return (filter.mask & (1 << (8 + i))) != 0;
    return false;
}
// 선택된 카테고리가 하나면 그 이름, 여러 개면 개수 요약 문구.
inline std::string selectionSummary(MapSelection const& filter) {
    int categories = 0, last = 0;
    for (int i = 0; i < 10; ++i)
        if (filter.mask & CATEGORY_MASKS[i]) {
            ++categories;
            last = i;
        }
    return categories == 1 ? RANDOM_DIFFICULTIES[last] : std::to_string(categories) + " difficulties selected";
}
// 룰렛 미리보기용 항목: 카테고리와 그에 해당하는 별 표기(예: 4/5).
struct RandomFilterPreview {
    int category;
    std::string stars;
};
// 랜덤 필터에서 켜진 카테고리별 별 수 목록을 만든다. 랜덤이 아니거나 필터가 잘못이면 빈 목록.
inline std::vector<RandomFilterPreview> randomFilterPreview(MapSelection const& filter) {
    std::vector<RandomFilterPreview> result;
    if (!filter.random || !validSelection(filter)) return result;
    for (int category = 0; category < 10; ++category) {
        int bits = filter.mask & CATEGORY_MASKS[category];
        if (!bits) continue;
        std::string stars;
        for (int bit = 0; bit < 13; ++bit)
            if (bits & (1 << bit)) {
                if (!stars.empty()) stars += "/";
                stars += std::to_string(bit < 8 ? bit + 2 : 10);
            }
        result.push_back({category, std::move(stars)});
    }
    return result;
}
// GD 검색에 보낼 질의 한 건: 난이도 값과 Demon 필터 값.
struct RandomMapQuery {
    int difficulty;
    int demonFilter;
};
// mask를 GD 검색 질의 목록으로 바꾼다: 일반 난이도 5가지는 각각, Demon은 세부 필터로.
inline std::vector<RandomMapQuery> randomMapQueries(MapSelection const& filter) {
    std::vector<RandomMapQuery> result;
    constexpr int nativeMasks[] = {1, 2, 12, 48, 192};
    for (int i = 0; i < 5; ++i)
        if (filter.mask & nativeMasks[i]) result.push_back({i + 1, 0});
    for (int i = 0; i < 5; ++i)
        if (filter.mask & (1 << (8 + i))) result.push_back({-2, i + 1});
    return result;
}
// 방의 한쪽 승수. 현재 끝난 배틀이 아직 집계되지 않았고 그 쪽이 이겼으면 +1을 미리 반영한다.
inline int roomWins(RoomInfo const& room, bool host) {
    if (!room.guest) return 0;
    int wins = host ? room.hostWins : room.guestWins;
    if (room.battle && room.battle->finishedAt > 0 && room.scoredMatch != room.battle->id && !room.battle->draw &&
        room.battle->winnerUid == (host ? room.host.uid : room.guest->uid))
        ++wins;
    return wins;
}
// 맵 룰렛이 아직 도는 중(확정 전)인지.
inline bool drawingMap(RoomInfo const& room) {
    return room.mapDraw && !room.mapDraw->settled;
}
// 방이 시작 전이고 랜덤이며 룰렛이 확정된 경우에만 맵을 다시 뽑을 수 있다.
inline bool canRerollMap(RoomInfo const& room) {
    return !room.started && room.mapSelection.random && room.mapDraw && room.mapDraw->settled;
}
// Map Pool에서 랜덤 추첨이 의미 있는지(랜덤 설정이고 맵이 2개 이상).
inline bool poolRolls(MapPool const& pool) {
    return pool.settings.random && pool.maps.size() >= 2;
}
// 방이 맵 추첨을 필요로 하는지(풀 랜덤이고 맵이 비었거나, 랜덤 필터이고 아직 확정 전).
inline bool needsRandomDraw(RoomInfo const& room) {
    if (room.mapPool) return poolRolls(*room.mapPool) && room.level.id <= 0;
    return room.mapSelection.random && (!room.mapDraw || !room.mapDraw->settled);
}
// 서버 시각 now 기준으로 룰렛 연출이 끝나기 전인지.
inline bool rouletteSpinning(std::optional<MapDraw> const& draw, int64_t now) {
    return draw.has_value() && now < draw->at + DRAW_END_MS;
}
// 룰렛 연출 중에는 확정된 결과를 감춘다: 레벨을 비우고 settled를 해제한다.
// 풀이면 당첨 맵을 룰렛 목록(levels)상의 위치에 맞게 풀 안에서 재배치한다.
template <class Room> bool hideSpinningDraw(Room& room, int64_t now) {
    if (!rouletteSpinning(room.mapDraw, now) || !room.mapDraw->settled) return false;
    room.level = {};
    room.mapDraw->settled = false;
    auto const& levels = room.mapDraw->levels;
    int const selected = room.mapDraw->selected;
    if (room.mapPool && selected >= 0 && selected < static_cast<int>(levels.size())) {
        auto& maps = room.mapPool->maps;
        auto const find = [&maps](int id) {
            return std::find_if(maps.begin(), maps.end(), [id](auto const& entry) { return entry.level.id == id; });
        };
        if (auto drawn = find(levels[selected].id); drawn != maps.end()) {
            auto entry = *drawn;
            maps.erase(drawn);
            auto at = maps.begin();
            if (selected > 0) {
                if (auto before = find(levels[selected - 1].id); before != maps.end()) at = before + 1;
            } else if (levels.size() > 1) {
                if (auto after = find(levels[1].id); after != maps.end()) at = after;
            }
            maps.insert(at, std::move(entry));
        }
    }
    return true;
}
// 룰렛 위치(칸 단위). 회전 시작 후 ease-out으로 count*5+selected 칸만큼 이동해 선택 칸에 멈춘다.
inline double roulettePosition(int count, int selected, int64_t elapsed) {
    if (count <= 0) return 0;
    double t = std::clamp(double(elapsed - DRAW_LEAD_MS) / DRAW_SPIN_MS, 0., 1.);
    return (count * 5. + selected) * (1. - std::pow(1. - t, 3.));
}
}
