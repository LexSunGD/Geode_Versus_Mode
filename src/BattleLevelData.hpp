// GD 레벨 데이터 문자열(';'로 레코드 구분, 첫 레코드는 레벨 설정)을 다루는 순수 헬퍼 모음.
// Start Pos 제거, Platformer 판별, 속도 포탈 기반 플레이 시간 추정, 길이 분류 검사를 제공한다.
#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
namespace versus {
// Start Pos 오브젝트(ID 31) 레코드를 제거한 레벨 데이터를 돌려준다. 첫 레코드(설정)는 그대로 둔다.
inline std::string withoutStartPositions(std::string_view data) {
    std::string result;
    result.reserve(data.size());
    bool settings = true;
    for (size_t begin = 0; begin < data.size();) {
        auto end = data.find(';', begin);
        if (end == std::string_view::npos) end = data.size();
        auto record = data.substr(begin, end - begin);
        bool start = false;
        if (!settings)
            for (size_t i = 0; i < record.size();) {
                auto comma = record.find(',', i);
                if (comma == std::string_view::npos) break;
                auto next = record.find(',', comma + 1);
                if (next == std::string_view::npos) next = record.size();
                if (record.substr(i, comma - i) == "1" && record.substr(comma + 1, next - comma - 1) == "31")
                    start = true;
                i = next + 1;
            }
        if (!start) {
            result.append(record);
            if (end < data.size()) result += ';';
        }
        settings = false;
        begin = end + 1;
    }
    return result;
}
// 레벨 설정 레코드의 kA22가 1이면 Platformer 레벨이다.
inline bool platformerLevelData(std::string_view data) {
    auto settings = data.substr(0, data.find(';'));
    for (size_t i = 0; i < settings.size();) {
        auto comma = settings.find(',', i);
        if (comma == std::string_view::npos) break;
        auto next = settings.find(',', comma + 1);
        if (next == std::string_view::npos) next = settings.size();
        if (settings.substr(i, comma - i) == "kA22") return settings.substr(comma + 1, next - comma - 1) == "1";
        i = next + 1;
    }
    return false;
}
// Platformer 맵을 거부할 때 보여 주는 메시지.
inline constexpr char const* kPlatformerMapMessage =
    "This map is a Platformer level. Versus supports Classic maps only.";

// 속도 값(0=보통, 1=느림, 2~4=빠름 단계)별 초당 이동 거리(유닛).
inline double speedUnits(int speed) {
    switch (speed) {
    case 1: return 251.16;
    case 2: return 387.42;
    case 3: return 468.0;
    case 4: return 576.0;
    default: return 311.58;
    }
}
// 속도 포탈 오브젝트 ID를 속도 값으로 바꾼다. 속도 포탈이 아니면 -1.
inline int portalSpeed(int id) {
    switch (id) {
    case 200: return 1;
    case 201: return 0;
    case 202: return 2;
    case 203: return 3;
    case 1334: return 4;
    default: return -1;
    }
}
// 부호와 소수점만 허용하는 간단한 숫자 파서(최대 32자). 지수 표기 등은 실패로 처리한다.
inline bool parseNumber(std::string_view text, double& out) {
    if (text.empty() || text.size() > 32) return false;
    double value = 0., scale = 0.;
    bool negative = false, digits = false;
    size_t i = 0;
    if (text[0] == '-') {
        negative = true;
        i = 1;
    }
    for (; i < text.size(); ++i) {
        char const c = text[i];
        if (c >= '0' && c <= '9') {
            digits = true;
            if (scale > 0.) {
                value += (c - '0') * scale;
                scale /= 10.;
            } else
                value = value * 10. + (c - '0');
        } else if (c == '.' && scale == 0.)
            scale = .1;
        else
            return false;
    }
    if (!digits) return false;
    out = negative ? -value : value;
    return true;
}
// 레벨 데이터에서 플레이 시간(초)을 추정한다.
// 시작 속도(kA4)와 속도 포탈 위치로 구간별 이동 시간을 더하고, 가장 먼 오브젝트 x를 끝으로 본다.
// 판단할 수 없으면 -1.
inline double levelDataSeconds(std::string_view data) {
    auto const split = data.find(';');
    if (split == std::string_view::npos) return -1.;
    int speed = 0;
    auto settings = data.substr(0, split);
    for (size_t i = 0; i < settings.size();) {
        auto comma = settings.find(',', i);
        if (comma == std::string_view::npos) break;
        auto next = settings.find(',', comma + 1);
        if (next == std::string_view::npos) next = settings.size();
        if (settings.substr(i, comma - i) == "kA4") {
            double value = 0.;
            if (parseNumber(settings.substr(comma + 1, next - comma - 1), value) && value >= 0. && value <= 4.)
                speed = static_cast<int>(value);
        }
        i = next + 1;
    }
    struct Portal {
        double x;
        int speed;
    };
    std::vector<Portal> portals;
    double end = 0.;
    for (size_t begin = split + 1; begin < data.size();) {
        auto stop = data.find(';', begin);
        if (stop == std::string_view::npos) stop = data.size();
        auto record = data.substr(begin, stop - begin);
        double id = -1., x = -1.;
        for (size_t i = 0; i < record.size();) {
            auto comma = record.find(',', i);
            if (comma == std::string_view::npos) break;
            auto next = record.find(',', comma + 1);
            if (next == std::string_view::npos) next = record.size();
            auto const key = record.substr(i, comma - i);
            auto const value = record.substr(comma + 1, next - comma - 1);
            if (key == "1")
                parseNumber(value, id);
            else if (key == "2")
                parseNumber(value, x);
            i = next + 1;
        }
        if (id > 0. && x >= 0.) {
            end = std::max(end, x);
            if (int const portal = portalSpeed(static_cast<int>(id)); portal >= 0) portals.push_back({x, portal});
        }
        begin = stop + 1;
    }
    if (end <= 0.) return -1.;
    std::sort(portals.begin(), portals.end(), [](Portal const& a, Portal const& b) { return a.x < b.x; });
    double seconds = 0., at = 0.;
    for (auto const& portal : portals) {
        if (portal.x >= end) break;
        if (portal.x > at) {
            seconds += (portal.x - at) / speedUnits(speed);
            at = portal.x;
        }
        speed = portal.speed;
    }
    seconds += (end - at) / speedUnits(speed);
    return seconds;
}
// 추정 시간이 GD 길이 분류(0~4)의 범위에 맞는지 검사한다. 경계에서 -20%/+25% 여유를 둔다.
// 길이 값이 범위 밖이면 시간이 양수이기만 하면 통과.
inline bool timeMatchesLength(double seconds, int length) {
    static constexpr double bounds[] = {0., 10., 30., 60., 120., 1e9};
    if (length < 0 || length > 4) return seconds > 0.;
    return seconds >= bounds[length] * .8 && seconds < bounds[length + 1] * 1.25;
}
// 초를 "45s" 또는 "2m 5s" 형태로 표시한다.
inline std::string playTimeText(double seconds) {
    int const total = static_cast<int>(seconds + .5);
    if (total < 60) return std::to_string(total) + "s";
    return std::to_string(total / 60) + "m " + std::to_string(total % 60) + "s";
}
}
