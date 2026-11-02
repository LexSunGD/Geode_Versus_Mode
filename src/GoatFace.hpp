// "G.O.A.T" 프로필 얼굴 / 불꽃 띠 생성 인터페이스 (구현은 GoatFace.cpp).
#pragma once
#include <Geode/cocos/include/cocos2d.h>
#include <string_view>

namespace versus::goat {
// 현재 이름과 이전 이름. isGoat는 look 문자열(@ 앞부분)이 둘 중 하나인지 확인한다.
inline constexpr std::string_view kName = "G.O.A.T";
inline constexpr std::string_view kOldName = "Grandpa Demon";
bool isGoat(std::string_view look);
// face: 얼굴 이미지(fire면 불꽃 포함), band: 티어 이름 뒤에 까는 가로 불꽃 띠. 만들 수 없으면 nullptr.
cocos2d::CCSprite* face(cocos2d::CCSize size, bool fire);
cocos2d::CCSprite* band(cocos2d::CCSize size, cocos2d::ccColor3B tint, float strength);
}
