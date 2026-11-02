// Cosmetics를 JSON으로 저장/복원하는 헬퍼.
#pragma once
#include "Cosmetics.hpp"
#include <matjson.hpp>

namespace versus {
// 값을 검증한 뒤 JSON 객체로 만든다.
inline matjson::Value cosmeticsJson(Cosmetics value) {
    value = sanitizeCosmetics(value);
    auto json = matjson::Value::object();
    json["start"] = value.start;
    json["end"] = value.end;
    json["angle"] = value.angle;
    json["aura"] = value.aura;
    json["execution"] = value.execution;
    json["title"] = value.title;
    json["font"] = value.font;
    return json;
}
// JSON에서 꾸미기 값을 읽는다. 정수가 아니거나 범위를 벗어난 필드는 fallback 값을
// 유지한다.
inline Cosmetics parseCosmetics(matjson::Value const& json, Cosmetics fallback = {}) {
    Cosmetics result = fallback;
    if (!json.isObject()) return result;
    auto read = [&json](char const* key, int fallback, int maximum) {
        auto const number = json[key].asInt().unwrapOr(-1);
        auto const real = json[key].asDouble().unwrapOr(-1.);
        return number >= 0 && number <= maximum && real == static_cast<double>(number) ? static_cast<int>(number)
                                                                                       : fallback;
    };
    result.start = read("start", result.start, 0xFFFFFF);
    result.end = read("end", result.end, 0xFFFFFF);
    result.angle = read("angle", result.angle, 359);
    result.aura = read("aura", result.aura, kAuraCount - 1);
    result.execution = read("execution", result.execution, kExecutionCount - 1);
    result.title = read("title", result.title, kS1FinalTitleBase + 109'999);
    if (!validTitle(result.title)) result.title = fallback.title;
    result.font = read("font", result.font, kNicknameFontCount - 1);
    return result;
}
}
