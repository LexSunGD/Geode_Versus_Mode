// 매치메이킹 대기 중에 띄우는 미니 러너 게임 레이어 SpectatorRunner의 선언.
// 물리는 RunnerPhysics.hpp, 상태는 cpp의 Impl에 둔다.
#pragma once
#include "VersusService.hpp"
#include <Geode/Geode.hpp>
#include <functional>
namespace versus {
// 화면 위에 겹쳐 뜨는 장애물 피하기 러너. 터치/키로 점프하며 매치메이킹 모드에서는 닫기/게임 전환 버튼이 붙는다.
class SpectatorRunner : public cocos2d::CCLayer {
    struct Impl;
    std::unique_ptr<Impl> m;
    bool init(PlayerProfile const&);
    void stopRunner();
    void switchGame(cocos2d::CCObject*);

public:
    SpectatorRunner();
    ~SpectatorRunner();
    static SpectatorRunner* create(PlayerProfile const&);
    // 매치메이킹 모드로 전환한다. switchGame은 게임 전환 버튼 콜백, closed는 닫힐 때 호출.
    void setMatchmakingMode(std::function<void()> switchGame = {}, std::function<void()> closed = {});
    // 러너를 멈추고 레이어를 제거한다. closed 콜백이 있으면 호출한다.
    void dismiss();
    void onEnter() override;
    void onExit() override;
    void closeMatchmaking(cocos2d::CCObject*);
    void update(float) override;
    void registerWithTouchDispatcher() override;
    bool ccTouchBegan(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void ccTouchEnded(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void ccTouchCancelled(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void keyDown(cocos2d::enumKeyCodes, double) override;
    void keyUp(cocos2d::enumKeyCodes, double) override;
    void keyBackClicked() override;
};
}
