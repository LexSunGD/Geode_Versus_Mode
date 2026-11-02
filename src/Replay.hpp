// 리플레이 모듈의 공개 인터페이스. 구현은 Replay.cpp.
// 세 부분으로 나뉜다: 매치 중 녹화/업로드, 저장된 리플레이 재생(watch), 라이브 중계와 관전(live*).
#pragma once
#include "ReplayFormat.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class PlayLayer;
class GJBaseGameLayer;

namespace versus {
struct LiveTarget;
}
namespace versus::replay {

// 이번 배틀의 레벨 정보(id, 버전, 데이터 해시, 저사양 여부)를 녹화기에 알린다.
void noteBattleLevel(std::int64_t levelId, int levelVersion, std::uint64_t hash, bool lowDetail);
// 매치 녹화 시작 (enabled: 업로드용 녹화, live: 라이브 전송용 녹화).
void beginMatch(PlayLayer* layer, std::string matchId, bool enabled, bool live = false);
// 시도 시작 전 준비 / 시도 녹화 시작. run은 시도 번호.
void prepareAttempt(PlayLayer* layer, int run);
void beginAttempt(PlayLayer* layer, int run);
// 물리 step 시작 / 버튼 입력마다 호출되어 상태와 입력을 기록한다.
void stepStart(GJBaseGameLayer* layer, float stepDelta);
void input(GJBaseGameLayer* layer, bool down, int button, bool player1);
// 시도 종료 기록 / 매치 종료(가장 높은 %의 시도를 업로드) / 매치 중단(기록 폐기).
void endAttempt(int percent, End end);
void finishMatch();
void abandonMatch();

// 볼 리플레이를 가리키는 정보 (매치 ID, 플레이어 uid, 표시 이름).
struct ReplayInfo {
    std::string matchId, uid, name;
};
// 리플레이 재생 시작 / 재생 중인지 / 재생 준비 중인지 / 뷰어에서 나가기.
void watch(ReplayInfo info);
bool viewing();
bool preparing();
bool leaveViewer();

// 내 시도를 라이브로 중계 시작(me가 비어 있으면 중단) / 대상 플레이어의 중계를 관전 / 모두 중지.
void liveStream(LiveTarget const& me);
void liveWatch(PlayLayer* layer, LiveTarget const& target, std::string name);
void liveStop();
// 관전 대상이 지금 시도 중인지 알려 준다.
void liveTargetActive(bool active);
// 관전 상태 조회: 연결 경고 문구, 보고 있는 uid, layer가 관전 중인 레이어인지, 클리어 장면을 재생 중인지,
// 재생 중인지, 화면 상단 캡션.
std::string liveWarning();
std::string liveWatchedUid();
bool liveWatching(GJBaseGameLayer const* layer);
bool liveHolding();
bool livePlaying();
std::string liveCaption();
}
