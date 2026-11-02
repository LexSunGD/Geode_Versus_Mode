// 매치(1v1/그룹/Ranked) 시작 절차와 배틀용 레벨 준비, 호환되지 않는 모드의 일시 정지,
// GD 진행 기록 보호 기능의 선언. 구현은 MatchLaunch.cpp의 LaunchController가 맡는다.
#pragma once
#include <cstdint>
#include <string>

class GJGameLevel;
class PlayLayer;

namespace versus {
// 방의 launch 정보로 매치 로딩(맵 다운로드, 리소스, PlayLayer 진입, 동기화 대기)을 시작한다.
void launchMatch();
// 로딩 절차가 진행 중이거나 이미 처리한 launch인지.
bool isMatchLaunchActive();
// 원본 레벨로 배틀용 레벨 복사본을 만든다(Start Pos 제거). 레벨 데이터 해시는 hash로 돌려준다.
GJGameLevel* createBattleLevel(GJGameLevel* source, bool lowDetail, std::uint64_t& hash);
// 배틀과 충돌하는 모드의 훅을 일시 정지한다.
void pauseBattleMods();
// 일시 정지 상태가 유지되는 동안의 PlayLayer를 등록한다.
void attachPausedMods(PlayLayer* layer);
// 해당 PlayLayer가 끝나면 일시 정지한 훅을 다시 켠다.
void releasePausedMods(PlayLayer* layer);
// 레벨의 GD 통계(시도 수, 최고 퍼센트 등)를 저장해 둔다.
void snapshotBattleLevelStats(int levelID);
// 맵이 로봇/스파이더 모드로 시작하면 그 스프라이트를 다시 만든다.
void refreshBattleStartMode(PlayLayer* layer);
// 저장해 둔 레벨 통계를 복원한다.
void restoreBattleLevelStats();
}
// 배틀 플레이가 GD 진행 기록에 반영되는 방식을 다루는 함수.
namespace versus::progress {
// 집계 대상 플레이를 종료로 확정한다.
void endCountedPlay(PlayLayer* layer);
// 기록하던 진행을 버린다. onlyOpen이면 아직 확정되지 않은 것만 버린다.
void discard(std::string const& why, bool onlyOpen = false);
// 이 PlayLayer가 배틀용 복사본이 아니라 실제 맵으로 열렸는지.
bool realMap(PlayLayer* layer);
// 별/오브 보상 기능 사용 여부(현재 꺼져 있음).
inline constexpr bool kStarsOrbsEnabled = false;
}
