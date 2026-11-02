// 그룹 배틀 중 탈락/종료한 플레이어(또는 LIVE 목록의 관전자)가 배틀 맵을 자유롭게 플레이하는
// free play 모드의 공개 인터페이스. 이 모드의 기록은 아무것도 반영되지 않으며,
// 다른 플레이어의 진행을 실시간으로 따라 보는 관전도 여기서 시작한다.
#pragma once
#include <string>

class GJGameLevel;
class GJBaseGameLayer;

namespace versus {
struct GroupBattle;
struct LiveMatch;
}
namespace versus::freeplay {
// 그룹 멤버가 배틀 맵(source)을 free play로 연다. watchUid가 있으면 해당 플레이어를 바로 관전한다.
bool start(GJGameLevel* level, std::string battleID, std::string watchUid = {}, std::string watchName = {});
// free play 중 특정 플레이어를 관전하도록 지정한다. 실패하면 false.
bool spectate(std::string const& uid, std::string const& name);
// 현재 관전 중인 플레이어의 uid(없으면 빈 문자열).
std::string spectatedUid();
// 지금 관전을 지정할 수 있는 상태인지.
bool canSpectate();
// free play 레이어가 열려 있는지(생성 중 포함).
bool active();
// 이 레이어가 free play의 PlayLayer인지.
bool owns(GJBaseGameLayer* layer);
// free play를 끝내고 이전 화면으로 돌아간다.
bool leave();

// LIVE 목록의 1v1 매치를 방 밖에서 free play + 관전으로 연다. 못 열면 why에 이유를 채운다.
bool openLive(LiveMatch const& match, std::string const& watchUid, std::string const& watchName, std::string& why);
// LIVE 목록의 그룹 배틀을 같은 방식으로 연다.
bool openLive(GroupBattle const& battle, std::string const& roomId, std::string const& roomName,
              std::string const& watchUid, std::string const& watchName, std::string& why);
// 관전할 만한 플레이어를 고른다(가장 멀리 간 진행 중 플레이어). 없으면 빈 문자열과 why.
std::string firstToWatch(GroupBattle const& battle, std::string& name, std::string& why);
// 해당 uid를 지금 관전할 수 있는지 검사한다.
bool canWatch(GroupBattle const& battle, std::string const& uid, std::string& why);
// LIVE 매치 정보를 GroupBattle 형태로 바꾼다.
GroupBattle battleOf(LiveMatch const& match);
// LIVE 보기를 여는 중인지(맵 다운로드 등).
bool opening();
// LIVE로 보고 있는 그룹 방 ID(그룹이 아니면 빈 문자열).
std::string liveRoom();
// LIVE로 보고 있는 방 이름.
std::string liveRoomName();
}
