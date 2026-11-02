// 진행 중인 1v1/그룹 배틀 세션(BattleSession.cpp의 Session 싱글턴)을 PlayLayer 훅들이 호출하는 인터페이스.
// 게임 훅은 이 함수들로 사망, 클리어, 리셋, 일시 정지, 키 입력, 종료를 세션에 알리고
// 입력/진행/일시 정지를 막을지 물어본다.
#pragma once
#include <string>
class PlayLayer;
class GJBaseGameLayer;
namespace cocos2d {
class CCNode;
class CCNodeRGBA;
}
namespace versus::battle {
// 관전/free play 상태를 알리는 테두리 프레임(SPECTATOR MODE 문구)을 parent에 붙인다.
cocos2d::CCNodeRGBA* addSpectatorFrame(cocos2d::CCNode* parent, int z, char const* caption);
// 라이브 관전 중이면 프레임 색과 문구를 관전용으로 바꾸고 경고 줄을 표시한다.
void showLiveState(cocos2d::CCNode* frame, GJBaseGameLayer* layer, std::string const& normal);
// 관전 버튼 배치 상수(바닥에서 높이, 크기, 버튼 간격, 오른쪽 여백)와 버튼이 차지하는 폭(문구 폭 계산용).
inline constexpr float kSpectateButtonY = 27.f, kSpectateButtonScale = .56f, kSpectateButtonGap = 5.f,
                       kSpectateButtonMargin = 14.f;
inline constexpr float kSpectateButtonsReserve = 200.f;
// 일시 정지 메뉴에 관전 모드 표시를 붙인다.
void markSpectatorPause(PlayLayer* layer);
// PlayLayer에서 배틀 세션을 시작한다(방 정보 읽기, HUD, 리플레이 기록 시작).
void begin(PlayLayer* layer);
// 세션을 해당 PlayLayer에서 분리한다(나갈 때).
void detach(PlayLayer* layer);
// 이 레이어가 진행 중인 배틀의 PlayLayer인지.
bool activeFor(GJBaseGameLayer* layer);
// 규칙의 배속(speed)을 반영한 시간 배율. 배틀이 아니면 1.
float speedFor(GJBaseGameLayer* layer);
// 오디오 피치를 원래 속도로 되돌린다.
void resetAudioSpeed();
// 물리 스텝 하나를 알려 TPS 측정에 쓴다.
void noteStep(GJBaseGameLayer* layer, float stepDelta);
// 그룹 배틀에서 결과가 확정된 플레이어의 일시 정지 메뉴인지.
bool groupSpectatorPause(GJBaseGameLayer* layer);
// free play 중 특정 플레이어를 실시간으로 관전하기 시작한다.
bool spectatePlayer(std::string const& uid, std::string const& name);
// 현재 관전 중인 플레이어의 uid.
std::string spectatedUid();
// 지금 관전을 시작할 수 있는지.
bool canSpectate();
// 치트가 감지되었을 때 실격 패배 처리한다(안티치트).
void disqualify(PlayLayer* layer, std::string const& reason, std::string const& integrityCode,
                std::string const& integrityFeatures);
// 무결성 보고가 밀려 있으면 즉시 서버에 보낸다.
void flushIntegrityReport(PlayLayer* layer);
// 로딩/종료/실격 상태 등으로 게임 진행, 입력, 일시 정지를 막아야 하는지.
bool blocksGameplay(GJBaseGameLayer* layer);
bool blocksInput(GJBaseGameLayer* layer);
bool blocksPause(GJBaseGameLayer* layer);
// 매 프레임 진행도(퍼센트)를 샘플링한다.
void sample(PlayLayer* layer);
// 시뮬레이션에 쓰인 시간을 누적한다.
void simulated(GJBaseGameLayer* layer, float dt);
// 사망 처리.
void died(PlayLayer* layer);
// 클리어 처리.
void completed(PlayLayer* layer);
// 리셋 전 호출. false면 리셋을 막는다.
bool beforeReset(PlayLayer* layer);
// 리셋 후 새 시도를 시작한다.
void afterReset(PlayLayer* layer);
// 일시 정지 상태 변화를 서버 진행 정보에 반영한다.
void paused(PlayLayer* layer, bool paused);
// Esc 키 처리. 일시 정지 길게 누르기 판정에 쓰며 처리했으면 true.
bool escapeKey(GJBaseGameLayer* layer, bool down);
// 뒤로 가기 키 처리.
bool backKey(GJBaseGameLayer* layer);
// 길게 눌러 얻은 일시 정지 권한으로 호출 중인지.
bool pauseHoldGranted(GJBaseGameLayer* layer);
// 일시 정지 메뉴가 열려 있는지.
bool pauseMenuOpen(PlayLayer* layer);
// 연습 모드 전환이 허용되는지.
bool allowPracticeToggle(PlayLayer* layer, bool practice);
// free play 중 연습 모드를 바꿀 수 있는지.
bool practiceSwitchable(PlayLayer* layer);
// 나가기 요청 처리(확인 창 등). 세션이 처리했으면 true.
bool requestQuit(PlayLayer* layer);
// Ranked에서 맵 리롤 투표가 가능한지, 내가/상대가 투표했는지, 투표 보내기.
bool canVoteReroll(PlayLayer* layer);
bool rerollVoted(PlayLayer* layer);
bool opponentRerollVoted(PlayLayer* layer);
void voteReroll(PlayLayer* layer);
}
