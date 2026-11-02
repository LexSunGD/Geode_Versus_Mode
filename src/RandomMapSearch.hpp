// 랜덤 맵 후보를 GD 온라인 검색으로 모으는 함수의 선언.
// MapSelection 필터와 추가 조건(RandomMapExtras)에 맞는 레벨 목록을 비동기로 받는다.
#pragma once
#include "VersusService.hpp"
namespace versus {
// 결과 콜백: 후보 레벨 목록과 오류 메시지(성공이면 빈 문자열).
using RandomMapsCallback = std::function<void(std::vector<LevelInfo>, std::string)>;
// MapSelection 외에 추가로 거는 조건. 길이/피처 상태/지정 ID 목록, 최소 후보 수,
// 최소 좋아요, 최대 플레이 시간(초), 2인 모드 제외/한정.
struct RandomMapExtras {
    std::vector<int> lengths;
    std::vector<int> features;
    std::vector<int> ids;
    int minimum = 0;
    int minLikes = 0;
    int maxSeconds = 0;
    bool noTwoPlayer = false;
    bool onlyTwoPlayer = false;
    bool operator==(RandomMapExtras const&) const = default;
};
// 방의 필터에 맞는 랜덤 맵 후보를 찾는다.
void findRandomMaps(MapSelection filter, RandomMapsCallback callback);
// 추가 조건을 함께 적용해 찾는다.
void findRandomMaps(MapSelection filter, RandomMapExtras extras, RandomMapsCallback callback);
// 방 없이 쓰는 검색. alive()가 false가 되면 검색을 중단한다.
void findRandomMapsWithoutRoom(MapSelection filter, std::function<bool()> alive, RandomMapsCallback callback);
}
