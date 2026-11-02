// 방에서 맵을 고를 때 GD 기본 레벨 검색 화면을 빌려 쓰는 기능의 선언.
// 일반 선택(호스트)과 Map Pool 추가용 선택을 지원한다.
#pragma once
#include "VersusService.hpp"
#include <functional>
class GJGameLevel;
namespace versus {
// GJGameLevel을 서버에 보낼 LevelInfo로 변환한다.
LevelInfo describeLevel(GJGameLevel* level);
// GD 검색 화면을 열어 레벨을 고르게 한다(호스트만). 선택하면 콜백을 호출하고 방 화면으로 돌아온다.
void openLevelSearch(std::function<void(LevelInfo)> onSelected);
// Map Pool 조건을 보여 주며 검색 화면을 연다. 콜백의 bool은 선택한 맵이 2인 모드인지 여부.
void openPoolSearch(MapPoolSettings settings, std::function<void(LevelInfo, bool)> onSelected);
// 검색 화면 선택 모드가 켜져 있는지.
bool isLevelSearchActive();
// 선택 모드를 끝내고 검색 화면 장면 스택을 원래 깊이로 되돌린다.
void cancelLevelSearch();
}
