// 처형(finisher) 연출 인터페이스. 구현은 ExecutionEffects.cpp.
#pragma once
#include <Geode/cocos/include/cocos2d.h>
namespace versus::execution {
// stage 위에서 kind번 처형 연출을 재생한다. winner가 loser에게 연출을 가한다.
// startDelay(초) 뒤에 시작한다.
void play(cocos2d::CCNode* stage, cocos2d::CCNode* winner, cocos2d::CCNode* loser, int kind, float startDelay = .55f);
// kind번 연출의 전체 길이(초).
float length(int kind);
}
