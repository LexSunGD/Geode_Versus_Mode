// 키 입력(뒤로가기 등) 처리 중에 바로 화면을 닫지 않고 다음 메인 스레드 틱으로 미루는
// 헬퍼.
#pragma once
#include <Geode/loader/Loader.hpp>
#include <Geode/utils/cocos.hpp>

#include <functional>

namespace versus {
// action을 메인 스레드 큐에 넣는다. owner를 Ref로 붙잡아 두고,
// 실행 시점에 owner가 아직 실행 중(isRunning)일 때만 호출한다.
inline void deferBack(cocos2d::CCNode* owner, std::function<void()> action) {
    if (!owner || !action) return;
    geode::Ref<cocos2d::CCNode> keep(owner);
    geode::Loader::get()->queueInMainThread([keep, action = std::move(action)] {
        if (keep->isRunning()) action();
    });
}
}
