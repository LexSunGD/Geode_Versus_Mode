// 방 안에서 이어지는 연속 대전의 누적 스코어(host/guest 승수) 처리.
// 방 JSON(matjson)의 "score" 항목을 초기화하거나 끝난 대전 결과를 반영한다.
#pragma once
#include <matjson.hpp>
#include <string>
#include <utility>

namespace versus {
// 스코어를 0:0으로 되돌리고 마지막 반영 대전 id를 비운다.
inline void resetRoomScore(matjson::Value& room) {
    auto score = matjson::Value::object();
    score["host"] = 0;
    score["guest"] = 0;
    score["lastMatch"] = "";
    room["score"] = std::move(score);
}
// 끝난 대전의 승자에게 1승을 더한다. 대전이 끝나지 않았거나 guest가 없거나
// 같은 대전 id가 이미 반영된 경우(lastMatch)는 무시해 중복 집계를 막는다. 무승부는 점수 변화 없음.
inline void settleRoomScore(matjson::Value& room) {
    auto const& snapshot = std::as_const(room);
    auto const& battle = snapshot["battle"];
    if (battle["finishedAt"].asInt().unwrapOr(0) <= 0 || !snapshot["guest"].isObject()) return;
    auto const id = battle["id"].asString().unwrapOr("");
    if (id.empty() || snapshot["score"]["lastMatch"].asString().unwrapOr("") == id) return;
    auto score = matjson::Value::object();
    int host = snapshot["score"]["host"].asInt().unwrapOr(0);
    int guest = snapshot["score"]["guest"].asInt().unwrapOr(0);
    auto winner = battle["winnerUid"].asString().unwrapOr("");
    if (!winner.empty() && !battle["draw"].asBool().unwrapOr(false)) {
        if (winner == snapshot["host"]["uid"].asString().unwrapOr(""))
            ++host;
        else if (winner == snapshot["guest"]["uid"].asString().unwrapOr(""))
            ++guest;
    }
    score["host"] = host;
    score["guest"] = guest;
    score["lastMatch"] = id;
    room["score"] = std::move(score);
}
}
