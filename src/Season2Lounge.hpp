// Season 2 lounge / plaza 장면의 공개 인터페이스. 실제 GD 플랫포머 레벨(PlayLayer)을
// 화면 안의 호스트 노드에 띄워 배경 장면으로 쓰고, 카메라 / 포즈 / 좌표 변환 등을 제공한다.
// 맵(레벨)과 음악 / 효과음 다운로드 관리도 여기서 한다 (구현은 Season2Lounge.cpp).
#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cocos2d {
class CCNode;
class CCPoint;
}
class GJBaseGameLayer;
class PlayLayer;

namespace versus::lounge {
// 호스트가 UI에 묻는 콜백: 뒤로 가기, 입력 허용 여부, 붙여도 되는지, 터치가 비어 있는지.
struct Hooks {
    std::function<void()> back;
    std::function<bool()> inputAllowed;
    std::function<bool()> attachAllowed;
    std::function<bool(float, float)> touchFree;
};
// 호스트 노드 생성 / 활성 여부 / 이 레이어가 lounge 소유인지 / 실제 PlayLayer / 해제.
cocos2d::CCNode* createHost(Hooks hooks);
bool active();
bool owns(GJBaseGameLayer const* layer);
PlayLayer* realPlayLayer();
void release();
// 호스트에 장식 노드(배지, 장식, 군중, 안내판)를 연결한다. crowdInLevel: 군중을 레벨 좌표계에 넣는지.
void setBadge(cocos2d::CCNode* host, cocos2d::CCNode* badge);
void setDecor(cocos2d::CCNode* host, cocos2d::CCNode* decor);
void setCrowd(cocos2d::CCNode* host, cocos2d::CCNode* crowd);
bool crowdInLevel(cocos2d::CCNode* host);
void setBillboard(cocos2d::CCNode* host, cocos2d::CCNode* board);
// 내 플레이어의 위치 / 회전 / 방향 (pose), 레벨 좌표 -> 호스트 좌표 변환 (toHost).
struct Pose {
    float x = 0.f, y = 0.f, rotation = 0.f;
    bool left = false;
};
bool pose(cocos2d::CCNode* host, Pose& out);
bool toHost(cocos2d::CCNode* host, cocos2d::CCPoint level, cocos2d::CCPoint& out);

// 맵 준비(prepareMap / mapReady), 컵별 레벨 ID 선택(mapLevel), 기본 노래 ID.
void prepareMap();
bool mapReady();
inline constexpr int kDemonLoungeLevelID = 151227082;
int mapLevel(std::string_view cup);
inline constexpr int kStandardCupSong = 10003659;
// 다운로드 상태: 맵(확인 중 / 받는 중 / 없음 / 준비됨)과 오디오(노래, 효과음 진행률).
struct MapDownload {
    enum State { Checking, Downloading, Missing, Ready };
    State state = Missing;
    int levelID = 0, version = -1;
    bool failed = false;
};
struct AudioDownload {
    int done = 0, total = 0, percent = 0;
    bool busy = false, failed = false;
};
struct Downloads {
    MapDownload standard, demon;
    AudioDownload song, sfx;
    bool retry = false;
};
// downloads: 현재 상태 조회, retryDownloads: 실패 재시도, resumeDownloads: 이어서 진행.
Downloads downloads();
void retryDownloads();
void resumeDownloads();
// 장면 제어: 즉시 붙이기, 시작 지점, 카메라 줌 / 초점 / 흔들림, 연출 모드, 입력 정지, 효과 정리,
// 방 기준점, 텔레포트, 방 그룹 스폰, 재시작, 빌드 횟수, 이동 대기, 방 색, 컨트롤 영역 판정.
void attachNow(cocos2d::CCNode* host);
bool spot(cocos2d::CCNode* host, cocos2d::CCPoint& out);
void camera(cocos2d::CCNode* host, float zoom, cocos2d::CCPoint focus, float toward, cocos2d::CCPoint shake);
void cinematic(cocos2d::CCNode* host, bool on);
void hold(cocos2d::CCNode* host, bool on);
void stopEffects(cocos2d::CCNode* host);
cocos2d::CCNode* effects(cocos2d::CCNode* host);
bool roomAnchor(cocos2d::CCNode* host, cocos2d::CCPoint& out);
void teleport(cocos2d::CCNode* host, cocos2d::CCPoint level);
void spawnRoomGroup(cocos2d::CCNode* host);
void restart(cocos2d::CCNode* host);
int builds(cocos2d::CCNode* host);
bool startSpot(cocos2d::CCNode* host, cocos2d::CCPoint& out);
bool pendingMove(cocos2d::CCNode* host, cocos2d::CCPoint& out);
float shift(cocos2d::CCNode* host, float dx);
void roomColors(cocos2d::CCNode* host, std::optional<std::pair<uint32_t, uint32_t>> colours);
bool onControls(cocos2d::CCNode* host, cocos2d::CCPoint screen);
// 에디터 Plaza Test용: 에디터 레벨 문자열을 맵으로 지정 / 해제 / 지정 여부.
bool setEditorMap(std::string const& text);
void clearEditorMap();
bool editorMap();
}
