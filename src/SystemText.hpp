// OS 시스템 폰트로 텍스트 스프라이트를 만드는 함수 선언 (BMFont에 없는 문자용). 구현은
// SystemText.cpp.
#pragma once
#include <Geode/cocos/include/cocos2d.h>

#include <string>

namespace versus {
// 시스템 텍스트 굵기.
enum class TextWeight { Regular, Bold };

// text를 그린 CCSprite를 만든다. width가 0이면 줄바꿈 폭 제한이 없다.
cocos2d::CCSprite* createSystemText(std::string const& text, float size, float width = 0.f,
                                    cocos2d::CCTextAlignment alignment = cocos2d::kCCTextAlignmentLeft,
                                    TextWeight weight = TextWeight::Regular);
// 기존 label 스프라이트의 텍스트를 교체한다.
void setSystemText(cocos2d::CCSprite* label, std::string const& text);
// 굵기에 맞는 시스템 폰트 이름.
char const* systemFontName(TextWeight weight = TextWeight::Regular);
}
