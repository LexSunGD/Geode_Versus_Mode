// Versus UI 의 화면 진입 함수 선언. 씬 생성(xxxScene)과 화면 전환(showXxx)을
// 각 UI 구현 파일 밖에서 호출할 수 있게 노출한다.
#pragma once
#include <string>
namespace cocos2d {
class CCScene;
}
namespace versus {
struct RoomInfo;
// 씬 생성 함수들. notice 는 화면 상단에 띄울 안내 문구.
cocos2d::CCScene* lobbyScene(std::string notice = {});
cocos2d::CCScene* roomScene();
cocos2d::CCScene* rankedScene(std::string notice = {}, bool fromBattle = false);
// 현재 씬을 해당 화면으로 전환하는 함수들
void showLobby(std::string notice = {});
void showVersus();
void showRoom();
void showRanked(std::string notice = {});
void showRankedRoulette(RoomInfo const& room);
void showSeason2();
void showGroupStandings();
// 점검 공지를 알림으로 표시한다. 표시했으면 true.
bool showMaintenanceNotice(std::string const& message);
// 서버 오류를 제목과 메시지로 알림창에 보여 준다.
void alertServerError(std::string const& title, std::string const& message);
}
