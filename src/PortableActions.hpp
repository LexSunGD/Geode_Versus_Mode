// 플랫폼 차이를 감춘 액션 헬퍼.
#pragma once
#include <Geode/Geode.hpp>

namespace versus {
// 노드를 즉시 position으로 옮기는 액션. iOS에서는 CCPlace 대신 시간 0의 CCMoveTo를 쓴다.
inline cocos2d::CCFiniteTimeAction* placeAction(cocos2d::CCPoint const& position) {
#ifdef GEODE_IS_IOS
    return cocos2d::CCMoveTo::create(0.f, position);
#else
    return cocos2d::CCPlace::create(position);
#endif
}
}
