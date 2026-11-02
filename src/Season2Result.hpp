// Season 2 매치 결과 연출 화면의 공개 인터페이스.
// Model에 양쪽 결과를 담아 show()로 띄운다 (구현은 Season2Result.cpp).
#pragma once
#include "VersusService.hpp"
#include <Geode/Geode.hpp>
#include <functional>
#include <string>

namespace versus::s2result {
// 한쪽 플레이어의 결과: 프로필, 최고 진행도(%), 사용한 시도 수, 이전 기록(before, -1이면 없음).
struct Side {
    PlayerProfile profile;
    int best = 0;
    int attemptsUsed = 0;
    int before = -1;
};
// 결과 화면 입력. left가 "나", leftWon / draw가 승패, reason은 기권 같은 부가 사유, seed는 execution 선택용.
struct Model {
    Side left, right;
    int lives = 0;
    bool draw = false;
    bool leftWon = false;
    std::string reason;
    std::string seed;
};
// 시험용 무작위 결과 (가짜 봇 상대).
Model randomBotModel();
// parent 위에 결과 화면을 띄운다. closed: 닫힌 뒤 호출, again: 있으면 AGAIN 버튼이 새 Model을 받아 다시 재생.
cocos2d::CCNode* show(cocos2d::CCNode* parent, Model const& model, std::function<void()> closed = {},
                      std::function<Model()> again = {});
}
