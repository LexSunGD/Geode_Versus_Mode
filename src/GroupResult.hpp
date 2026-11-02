// 그룹 배틀 종료 후 보여 주는 결과 화면(우승자 쇼케이스 + 순위표)의 선언.
#pragma once
#include "VersusService.hpp"
#include <Geode/Geode.hpp>
#include <map>
#include <string>

namespace versus {
// 결과 화면 노드를 만든다. before는 uid별 이전 맵 기록(최고 퍼센트)이며 순위표에 표시한다.
// 나가기 버튼은 exitLabel로 만들고 누르면 onExit를 호출한다.
cocos2d::CCNode* createGroupResult(GroupBattle const& battle, std::string const& selfUid, char const* exitLabel,
                                   cocos2d::CCObject* target, cocos2d::SEL_MenuHandler onExit,
                                   std::map<std::string, int> const* before = nullptr);
// 자동 복귀 카운트다운 문구와 진행 바를 갱신한다(remaining/total은 초).
void updateGroupResult(cocos2d::CCNode* result, float remaining, float total);
// 결과 화면이 자동으로 닫히기까지의 시간(초).
inline constexpr float kGroupResultSeconds = 30.f;
}
