// 대전 중 외부 치트(Mega Hack, Prism 등) 감시를 프레임 루프에 연결하는 파일.
// CCDirector::drawScene과 CCScheduler::update를 훅해서 주기적으로 검사하고,
// 감지되면 battle::disqualify로 실격 처리한다. 판정 로직 자체는 ExternalCheatMonitor에
// 있다.
#include "IntegrityMonitor.hpp"
#include "BattleSession.hpp"
#include "ExternalCheatMonitor.hpp"
#include "VersusService.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <chrono>

using namespace geode::prelude;

namespace versus::integrity {
namespace {
// 마지막으로 검사한 PlayLayer와 그 시각, drawScene 진행 중 여부.
using Clock = std::chrono::steady_clock;
PlayLayer* owner = nullptr;
Clock::time_point checked{};
bool drawing = false;

// 안티치트가 적용되는 상태에서 250ms에 한 번 외부 치트를 검사한다.
// PlayLayer가 바뀌면 타이머를 초기화하고, 감지 시 실격 처리한다.
void pollExternal(PlayLayer* play) {
    if (play != owner) {
        owner = play;
        checked = {};
    }
    auto const now = Clock::now();
    if (!play || !Service::get().antiCheatEnforced() ||
        (checked != Clock::time_point{} && now - checked < std::chrono::milliseconds(250)))
        return;
    checked = now;
    if (auto status = activeExternalCheat(false); status.detected)
        battle::disqualify(play, status.detail, status.code, status.features);
}
}

// drawScene이 진행 중이고 layer가 현재 PlayLayer인지 확인한다.
bool framePending(PlayLayer* layer) {
    return drawing && layer && layer == PlayLayer::get();
}

// 활성 대전 중인 현재 PlayLayer에 대해 타이머와 무관하게 즉시 검사한다.
void checkNow(PlayLayer* layer) {
    if (!layer || layer != PlayLayer::get() || !battle::activeFor(layer)) return;
    owner = layer;
    checked = {};
    pollExternal(layer);
}
}

// drawScene을 가장 먼저 가로채 그리기 전에 검사하고, 그린 뒤 무결성 리포트를 내보낸다.
class $modify(VersusIntegrityDirector, CCDirector){
    static void onModify(auto& self){(void)self.setHookPriorityPre("cocos2d::CCDirector::drawScene", Priority::First);
}
void drawScene() {
    // 대전 중이 아니면 상태만 초기화하고 그대로 그린다. 재진입 호출도 그대로 통과시킨다.
    using namespace versus::integrity;
    if (!versus::battle::activeFor(PlayLayer::get())) {
        owner = nullptr;
        checked = {};
        CCDirector::drawScene();
        return;
    }
    if (drawing) {
        CCDirector::drawScene();
        return;
    }
    drawing = true;
    pollExternal(PlayLayer::get());
    CCDirector::drawScene();
    drawing = false;
    versus::battle::flushIntegrityReport(PlayLayer::get());
}
}
;

// Prism Menu 훅보다 먼저 실행되어 update 구간의 시작/끝을 표시한다.
// 그 사이에 Prism이 바꾼 dt를 다음 훅에서 비교하기 위함이다.
class $modify(VersusPrismSchedulerScope, CCScheduler){
    static void onModify(auto& self){(void)self.setHookPriorityBeforePre("cocos2d::CCScheduler::update", "firee.prism");
}
void update(float dt) {
    versus::integrity::beginPrismSchedulerScope(this, dt);
    CCScheduler::update(dt);
    versus::integrity::endPrismSchedulerScope(this);
}
}
;

// 다른 모드(Mega Hack, Prism)의 훅이 모두 끝난 뒤 실행되도록 우선순위를 낮춰,
// 최종 dt가 비정상이면 실격 처리한다.
class $modify(VersusPrismSchedulerFeature, CCScheduler){
    static void onModify(auto& self){(void)self.setHookPriorityPre("cocos2d::CCScheduler::update", Priority::Last);
(void)self.setHookPriorityAfterPre("cocos2d::CCScheduler::update", "absolllute.megahack");
(void)self.setHookPriorityAfterPre("cocos2d::CCScheduler::update", "firee.prism");
}
void update(float dt) {
    if (auto status = versus::integrity::observePrismSchedulerDelta(this, dt); status.detected)
        versus::battle::disqualify(PlayLayer::get(), status.detail, status.code, status.features);
    CCScheduler::update(dt);
}
}
;
