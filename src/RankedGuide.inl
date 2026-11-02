// Ranked 매치의 승패 판정 규칙을 3단계(진행도 > 남은 생명 > 코인)로 설명하는 안내 팝업.
//
// "How to Win" 안내 팝업. 판정 우선순위 3줄과 보충 규칙(생명 비교, 일시정지 예산, 무승부)을 보여 준다.
class RankedGuide final : public ::versus::DeferredPopup {
    bool init() override {
        if (!Popup::init(400.f, 280.f)) return false;
        setTitle("How to Win", "bigFont.fnt", .6f);
        char const* headings[] = {"1  PROGRESS", "2  REMAINING LIVES", "3  USER COINS"};
        char const* details[] = {"Higher best percent wins.", "Same percent: fewer failed attempts wins.",
                                 "Same clear + attempts: more collected coins wins."};
        for (int i = 0; i < 3; ++i) {
            float y = 211.f - i * 53.f;
            panel(m_mainLayer, {15.f, y - 22.f}, {370.f, 47.f}, false);
            CCNode* icon = nullptr;
            if (i == 1)
                icon = createLifeHeart(24.f);
            else if (i == 2)
                icon = CCSprite::createWithSpriteFrameName("GJ_coinsIcon_001.png");
            else
                icon = CCLabelBMFont::create("%", "bigFont.fnt");
            if (icon) {
                icon->setPosition({40.f, y});
                if (i != 1) icon->setScale(.65f);
                m_mainLayer->addChild(icon);
            }
            label(m_mainLayer, headings[i], {76.f, y + 9.f}, .37f, 292.f, ccWHITE, true);
            label(m_mainLayer, details[i], {76.f, y - 10.f}, .45f, 292.f, kIce, true, "chatFont.fnt");
        }
        label(m_mainLayer, "All three equal = DRAW", {200.f, 57.f}, .4f, 360.f, kIce);
        label(
            m_mainLayer,
            "After A clears, B loses immediately if B has fewer lives left.\nPause budget: 20s total. Forfeit or timeout = LOSS.\nDraw = new map, same opponent. Lives: 2-25 (fewer on longer maps).",
            {200.f, 28.f}, .43f, 370.f, kIce, false, "chatFont.fnt");
        return true;
    }

public:
    CREATE_FUNC(RankedGuide);
};
