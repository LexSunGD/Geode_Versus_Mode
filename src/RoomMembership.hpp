// 1v1 방(matjson)의 guest 입장/퇴장 관련 상태 규칙.
// guest 유무·시간 초과 판정, guest 제거 시 방 상태 초기화, 입장 가능 여부와 거절 사유를 다룬다.
#pragma once

#include "RoomScore.hpp"
#include <matjson.hpp>
#include <cstdint>
#include <string>

namespace versus {
// guest의 다운로드 진행 정보를 지우고, 비면 downloads 항목 자체를 없앤다.
inline void clearGuestDownloads(matjson::Value& room) {
    if (std::as_const(room)["downloads"].isObject()) {
        room["downloads"].erase("guest");
        if (room["downloads"].size() == 0) room.erase("downloads");
    }
}
// 방에 guest가 들어와 있는지.
inline bool hasRoomGuest(matjson::Value const& room) {
    return !room["guest"]["uid"].asString().unwrapOr("").empty();
}

// guest의 마지막 확인 시각(guestSeen)이 timeout 이상 지났는지.
inline bool roomGuestExpired(matjson::Value const& room, int64_t now, int64_t timeout) {
    return hasRoomGuest(room) && now - room["guestSeen"].asInt().unwrapOr(0) >= timeout;
}

// guest를 내보내고 방을 대기 상태로 돌린다: 준비/시작/전투/맵 추첨 정보와 스코어를 초기화하고,
// 무작위 맵이면 선택된 level을 비우며, 맵 풀의 permit 목록도 비운다(무작위 풀이면 level도 비움).
inline void removeRoomGuest(matjson::Value& room) {
    auto const& snapshot = std::as_const(room);
    auto const guestUid = snapshot["guest"]["uid"].asString().unwrapOr("");
    if (!guestUid.empty() && snapshot["emote"]["uid"].asString().unwrapOr("") == guestUid) room.erase("emote");
    room.erase("guest");
    room.erase("guestSeen");
    clearGuestDownloads(room);
    room["started"] = false;
    room["hostReady"] = false;
    room["guestReady"] = false;
    room.erase("launch");
    room.erase("battle");
    room.erase("mapDraw");
    if (snapshot["mapSelection"]["random"].asBool().unwrapOr(false)) room.erase("level");
    if (snapshot["mapPool"].isObject()) {
        bool const rolls = snapshot["mapPool"]["random"].asBool().unwrapOr(false) &&
                           snapshot["mapPool"]["maps"].isArray() && snapshot["mapPool"]["maps"].size() >= 2;
        room["mapPool"]["permit"] = matjson::Value::array();
        if (rolls) room.erase("level");
    }
    resetRoomScore(room);
}

// 입장 불가 사유 문구를 돌려준다(빈 문자열이면 입장 가능). 내 방, 방장 연결 끊김,
// 이미 guest인 경우(재입장 허용), 시작됨, 가득 참 순으로 검사한다.
inline std::string roomJoinError(matjson::Value const& room, std::string const& uid, int64_t now, int64_t timeout) {
    if (room["host"]["uid"].asString().unwrapOr("") == uid) return "This is already your room.";
    if (now - room["hostSeen"].asInt().unwrapOr(0) >= timeout) return "The host disconnected.";
    if (room["guest"]["uid"].asString().unwrapOr("") == uid) return {};
    if (room["started"].asBool().unwrapOr(false)) return "The match has already started.";
    if (hasRoomGuest(room)) return "This room is full.";
    return {};
}
}
