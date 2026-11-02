// 메뉴 화면에서 마우스 커서를 보이게 유지하는 인터페이스. 구현은 MenuCursor.cpp.
#pragma once

namespace cocos2d {
class CCNode;
}

namespace versus {
// owner 노드가 화면에 있는 동안 커서를 보이게 유지한다(Windows 전용 동작). 해제는
// releaseMenuCursor.
void keepMenuCursorVisible(cocos2d::CCNode* owner);
void releaseMenuCursor(cocos2d::CCNode* owner);
}

// 게임 설정에 따른 커서 표시/잠금 상태 조회와 커서 제어 함수들.
// unlock이 true이면 게임의 커서 잠금 설정을 무시한다.
namespace versus::cursor {
// 게임 설정에서 플레이 중 커서 표시가 켜져 있는지.
bool gameShowsCursor();
// 게임 설정에서 커서 잠금이 켜져 있는지.
bool gameLocksCursor();
// 커서를 보이게 한다. 잠금은 unlock이 아니고 게임 설정이 잠금일 때만 건다.
void show(bool unlock);
// 게임 설정에 맞게 커서 잠금을 다시 건다(또는 푼다).
void relock(bool unlock);
// 플레이 중에는 커서를 숨기고 필요하면 잠근다. 메뉴 커서가 필요한 상태면 건드리지 않는다.
void hideForPlay();
}
