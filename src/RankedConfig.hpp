// Ranked 모드의 클라이언트 쪽 상수와 순수 계산 함수 모음.
// 매치메이킹 탐색 범위, Elo 변동량, 연승 보너스, 맵 진행도에 따른 패배 감점 완화,
// 티어별 맵 풀 마스크와 생명 수, 순위표 표시 규칙, 매치 진행 단계(Phase)를 정의한다.
#pragma once

#include <array>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <utility>

namespace versus::ranked {
// 듀얼 상대로 지정할 수 있는 최소 레이팅(반올림 기준).
inline constexpr int DUEL_MIN_TARGET_RATING = 500;
inline bool duelTargetAllowed(double rating) {
    return std::floor(rating + .5) >= DUEL_MIN_TARGET_RATING;
}
// 듀얼 신청자가 상대보다 이 값 이상 낮으면 신청할 수 없다.
inline constexpr int DUEL_MAX_RATING_GAP = 500;
// 신청자(rating)가 상대(targetRating)에게 듀얼을 걸 수 있는지.
inline bool duelChallengerAllowed(double rating, double targetRating) {
    auto const mine = static_cast<long long>(std::floor(rating + .5));
    auto const theirs = static_cast<long long>(std::floor(targetRating + .5));
    return theirs - mine < DUEL_MAX_RATING_GAP;
}
// 배치 경기 수(0 = 배치 없음).
inline constexpr int PLACEMENT_MATCH_COUNT = 0;
// Glicko 계열 초기값: 레이팅, RD(불확실성), 변동성, 시스템 상수 tau.
inline constexpr double INITIAL_RATING = 1500.;
inline constexpr double INITIAL_RD = 350.;
inline constexpr double INITIAL_VOLATILITY = .06;
inline constexpr double GLICKO_TAU = .5;
// 순위표에 올라가기 위해 최근 이 일수 안에 경기를 해야 한다.
inline constexpr int ACTIVE_RANK_DAYS = 30;
// 맵 룰렛에서 보여 줄 후보 맵 개수.
inline constexpr int ROULETTE_CANDIDATE_COUNT = 8;
// 매치 상태 폴링 주기(초). 매칭 중 / 대기 중.
inline constexpr float STATE_POLL_SECONDS = 1.f;
inline constexpr float IDLE_STATE_POLL_SECONDS = 2.f;
// 다음 갱신에서 바로 폴링하게 만드는 큰 값.
inline constexpr float STATE_POLL_NOW = 1'000'000.f;
// 준비 완료 신호 재전송 간격(초)과 에셋 재시도 간격(초).
inline constexpr float READY_RESEND_SECONDS = 2.f;
inline constexpr float ASSET_RETRY_SECONDS = 2.f;
// 에셋 다운로드 제한 시간(ms).
inline constexpr int64_t ASSET_DOWNLOAD_TIMEOUT_MS = 60'000;
// 전투 시작 카운트다운(ms).
inline constexpr int64_t BATTLE_START_COUNTDOWN_MS = 3'000;
// 레이팅 변동 연출 시간(초)과 매치 인트로 길이(초).
inline constexpr float RATING_ANIMATION_SECONDS = 2.f;
inline constexpr float MATCH_INTRO_SECONDS = 4.5f;
// 탐색 범위 표(0~45초)가 끝나는 시각(ms)과 그 이후 시작 범위.
inline constexpr int64_t SEARCH_TABLE_END_MS = 45'000;
inline constexpr int SEARCH_TABLE_WIDEST = 400;
// 표 이후에는 이 시간(ms)마다 범위가 시작 범위만큼씩 선형으로 늘어난다. 상한은 SEARCH_RANGE_MAX.
inline constexpr int64_t LONG_WAIT_GROWTH_MS = 120'000;
inline constexpr int SEARCH_RANGE_MAX = 1'000'000;
// 대기 시간(ms)에 따른 매칭 허용 레이팅 차이. 10초 미만 100, 20초 150, 30초 200,
// 45초까지 300, 이후 400에서 시작해 선형 증가한다.
inline constexpr int matchmakingRange(int64_t waitMs) {
    if (waitMs < SEARCH_TABLE_END_MS)
        return waitMs < 10'000 ? 100 : waitMs < 20'000 ? 150 : waitMs < 30'000 ? 200 : 300;
    int64_t const over = waitMs - SEARCH_TABLE_END_MS;
    int64_t const value = SEARCH_TABLE_WIDEST + SEARCH_TABLE_WIDEST * over / LONG_WAIT_GROWTH_MS;
    return static_cast<int>(value < SEARCH_RANGE_MAX ? value : SEARCH_RANGE_MAX);
}

// Elo 기본 값: 기대 승률 계산 스케일, 신규 유저 판정 경기 수, 신규 유저 최소 K.
inline constexpr double ELO_SCALE = 400.;
inline constexpr int ELO_NEW_PLAYER_MATCHES = 10;
inline constexpr int ELO_K_NEW = 89;
// 레이팅이 below 미만일 때 적용하는 K값 구간.
struct EloBand {
    double below;
    int k;
};
inline constexpr std::array<EloBand, 6> ELO_K_BANDS = {{
    {1200., 110},
    {1500., 89},
    {1800., 66},
    {2100., 49},
    {2400., 38},
    {std::numeric_limits<double>::infinity(), 28},
}};
// 승리 시 최소 +2, 패배 시 최대 -2로 보정하는 하한.
inline constexpr int ELO_MIN_DELTA = 2;
// 더 높은 레이팅이 질 때(업셋) 감점에 곱해 완화하는 비율.
inline constexpr double ELO_UPSET_LOSS_SCALE = .5;
// 레이팅 구간별 K값.
inline int eloBandK(double rating) {
    for (auto const& band : ELO_K_BANDS)
        if (rating < band.below) return band.k;
    return ELO_K_BANDS.back().k;
}
// 신규 유저(경기 수 < ELO_NEW_PLAYER_MATCHES)는 구간 K와 ELO_K_NEW 중 큰 값을 쓴다.
inline int eloK(int matches, double rating) {
    int const band = eloBandK(rating);
    return matches < ELO_NEW_PLAYER_MATCHES ? std::max(band, ELO_K_NEW) : band;
}
// 상대 대비 기대 승률(0~1).
inline double eloExpected(double rating, double opponent) {
    return 1. / (1. + std::pow(10., (opponent - rating) / ELO_SCALE));
}
// 레이팅 차이가 크면 변동량을 줄이는 배율의 하한.
inline constexpr double RATING_GAP_FLOOR = .1;
// 레이팅 차이가 ELO_SCALE 이내면 1, 그보다 크면 ELO_SCALE/차이로 줄이되 하한을 둔다.
inline double ratingGapScale(double gap) {
    gap = std::fabs(gap);
    return gap <= ELO_SCALE ? 1. : std::max(RATING_GAP_FLOOR, ELO_SCALE / gap);
}
// 한 판 결과(score: 승 1, 무 .5, 패 0)에 따른 레이팅 변동. 높은 쪽이 질 때는 감점을 완화하고,
// 차이 배율을 곱해 반올림한 뒤 승리/패배 방향의 최소 변동을 보장한다.
inline int eloDeltaWithK(int k, double rating, double opponent, double score) {
    double const expected = eloExpected(rating, opponent);
    double raw = k * (score - expected);
    if (score == 0. && opponent < rating) raw = -k * (.5 + (expected - .5) * ELO_UPSET_LOSS_SCALE);
    double const unscaled = raw;
    raw *= ratingGapScale(rating - opponent);
    int delta = static_cast<int>(std::copysign(std::floor(std::fabs(raw) + .5), raw));
    if (delta == 0 && std::floor(std::fabs(unscaled) + .5) >= 1.) delta = raw > 0. ? 1 : -1;
    if (score == 1.)
        delta = std::max(delta, ELO_MIN_DELTA);
    else if (score == 0.)
        delta = std::min(delta, -ELO_MIN_DELTA);
    return delta;
}
// 경기 수로 K를 정해 eloDeltaWithK에 넘기는 래퍼.
inline int eloDelta(double rating, double opponent, double score, int matches) {
    return eloDeltaWithK(eloK(matches, rating), rating, opponent, score);
}
// 연승 보너스: 2연승부터 연승 수에 따라 STEP씩 늘고 MAX에서 멈춘다.
inline constexpr int STREAK_BONUS_STEP = 2, STREAK_BONUS_MAX = 8;
inline constexpr int streakBonus(int streak) {
    return streak < 2 ? 0 : std::min(STREAK_BONUS_MAX, STREAK_BONUS_STEP * (streak - 1));
}
// 맵 진행도로 줄일 수 있는 패배 감점의 최대 비율.
inline constexpr double MAP_LOSS_REDUCTION_MAX = .6;
// 맵 난이도 티어별로 감점 완화가 최대가 되는 진행도(%) 차이. 알 수 없는 티어는 100.
inline int mapFullGap(std::string_view tier) {
    constexpr std::pair<std::string_view, int> gaps[] = {
        {"Easy", 100},      {"Normal", 90},       {"Hard", 80},       {"Harder", 70},       {"Insane", 60},
        {"Easy Demon", 50}, {"Medium Demon", 40}, {"Hard Demon", 30}, {"Insane Demon", 20}, {"Extreme Demon", 10}};
    for (auto const& [name, gap] : gaps)
        if (name == tier) return gap;
    return 100;
}
// 승자와 패자의 최고 진행도(%) 차이를 티어별 기준 차이로 나눈 비율에
// 최대 감소율을 곱해 패배 감점 완화율을 구한다.
inline double mapLossReduction(int loserBest, int winnerBest, std::string_view tier = {}) {
    int const loser = std::clamp(loserBest, 0, 100), winner = std::clamp(winnerBest, 0, 100), full = mapFullGap(tier);
    return MAP_LOSS_REDUCTION_MAX * std::min(std::max(0, winner - loser), full) / static_cast<double>(full);
}
// 음수 변동에만 완화율을 적용한다. 반올림하되 최소 -1은 유지한다.
inline int reducedLoss(int delta, double reduction) {
    if (delta >= 0 || reduction <= 0.) return delta;
    double const raw = delta * (1. - reduction);
    return std::min(-1, static_cast<int>(std::copysign(std::floor(std::fabs(raw) + .5), raw)));
}

// 티어 이름과 해당 티어의 순위 구간 표시 문구.
struct TierDisplay {
    std::string_view name;
    std::string_view rankBand;
};

// 표시용 티어 목록(높은 티어부터)과 상위 퍼센트 구간.
inline constexpr std::array<TierDisplay, 10> TIER_DISPLAY = {{
    {"Extreme Demon", "TOP 1%"},
    {"Insane Demon", "TOP 1-5%"},
    {"Hard Demon", "TOP 5-8%"},
    {"Medium Demon", "TOP 8-11%"},
    {"Easy Demon", "TOP 11-15%"},
    {"Insane", "TOP 15-35%"},
    {"Harder", "TOP 35-50%"},
    {"Hard", "TOP 50-65%"},
    {"Normal", "TOP 65-82%"},
    {"Easy", "TOP 82-100%"},
}};
// 난이도 비트(2별 ~ Extreme Demon, 13단계)별 기본 생명 수와 표시 이름.
inline constexpr std::array<int, 13> MASK_LIVES = {3, 3, 4, 4, 6, 7, 9, 11, 12, 15, 18, 21, 25};
inline constexpr std::array<std::string_view, 13> MASK_DIFFICULTY = {
    "2 Stars", "3 Stars",    "4 Stars",      "5 Stars",    "6 Stars",      "7 Stars",      "8 Stars",
    "9 Stars", "Easy Demon", "Medium Demon", "Hard Demon", "Insane Demon", "Extreme Demon"};
// 티어별 맵 풀에 포함되는 난이도 비트 마스크(TIER_DISPLAY와 같은 순서).
inline constexpr std::array<int, 10> TIER_POOL_MASK = {
    2048 | 4096,    1024 | 2048 | 4096, 512 | 1024 | 2048, 256 | 512 | 1024, 128 | 256 | 512,
    64 | 128 | 256, 32 | 64 | 128,      8 | 16 | 32 | 64,  4 | 8 | 16 | 32,  4 | 8 | 16};
// 맵 길이에 따라 생명 수를 줄인다(길이 4는 절반, 3은 70%). xlLong이면 30%로 더 줄인다.
inline constexpr int livesFor(int base, int length, bool xlLong = false) {
    if (length == 4 && xlLong) return std::max(2, static_cast<int>(base * .3 + .5));
    double const scale = length == 4 ? .5 : length == 3 ? .7 : 1.;
    return std::max(3, static_cast<int>(base * scale + .5));
}
// 별 수 / 데몬 여부 / 난이도 값으로 기본 생명 수를 고른다.
inline constexpr int baseLives(int stars, bool demon, int difficulty) {
    if (demon) {
        switch (difficulty) {
        case 7: return MASK_LIVES[8];
        case 8: return MASK_LIVES[9];
        case 9: return MASK_LIVES[11];
        case 10: return MASK_LIVES[12];
        default: return MASK_LIVES[10];
        }
    }
    if (stars >= 2 && stars <= 9) return MASK_LIVES[stars - 2];
    switch (difficulty) {
    case 2: return MASK_LIVES[1];
    case 3: return MASK_LIVES[2];
    case 4: return MASK_LIVES[4];
    case 5: return MASK_LIVES[6];
    default: return MASK_LIVES[0];
    }
}
static_assert(baseLives(5, false, 3) == 4 && baseLives(10, true, 10) == 25 && baseLives(10, true, 6) == 18 &&
              baseLives(0, false, 5) == 9 && baseLives(1, false, -1) == 3 && baseLives(0, true, 8) == 15);
// 리롤 시 생명 계산에 쓰는 퍼센트.
inline constexpr int REROLL_LIVES_PERCENT = 40;
// 마스크에 포함된 난이도들의 생명 수 중 최소/최대를 돌려준다.
inline constexpr std::pair<int, int> poolLives(int mask) {
    int low = 0, high = 0;
    for (size_t bit = 0; bit < MASK_LIVES.size(); ++bit) {
        if (!(mask & (1 << bit))) continue;
        int const lives = MASK_LIVES[bit];
        low = low ? std::min(low, lives) : lives;
        high = std::max(high, lives);
    }
    return {low, high};
}
// 순위표 그래프의 레이팅 상한.
inline constexpr double BOARD_RATING_MAX = 3000.;
// 레이팅(0~2500)을 가로 폭 안의 x 좌표로 바꾼다(양쪽 14 여백).
inline float ratingAxisX(float width, double rating) {
    return 14.f + std::max(0.f, width - 28.f) * static_cast<float>(std::clamp(rating / 2500., 0., 1.));
}
// 순위별 배지 아이콘 프레임. 1~5 top10, ~50 top50, ~100 top500, 100위 밖은 없음.
inline constexpr char const* rankBadgeFrame(int rank) {
    if (rank <= 0 || rank > 100) return nullptr;
    if (rank <= 5) return "rankIcon_top10_001.png";
    if (rank <= 50) return "rankIcon_top50_001.png";
    return "rankIcon_top500_001.png";
}
// 정확한 순위 숫자를 보여 줄지(100 이상).
inline constexpr bool showExactRank(int rank) {
    return rank >= 100;
}

// Ranked 매치 진행 단계: 대기 -> 탐색 -> 매치 발견 -> 맵 룰렛 -> 에셋 준비 -> 상대 준비 대기
// -> 전투 씬 로딩 -> 시작 대기 -> 플레이 -> 복귀 -> 결과 연출(배치/레이팅/승급) 또는 취소.
enum class Phase {
    Idle,
    Searching,
    MatchFound,
    MapRoulette,
    AssetPreparing,
    WaitingOpponentReady,
    BattleSceneLoading,
    WaitingBattleStart,
    Playing,
    ReturningFromMatch,
    PlacementReveal,
    RankUpdate,
    RankPromotion,
    Canceled,
};
}
