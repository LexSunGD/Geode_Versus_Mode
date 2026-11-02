// 설정 팝업. 두 탭으로 구성된다.
// Sound: GD 음악/효과음 볼륨과 (랭크 화면에서) 랭크 로비 음악 볼륨.
// Battle Display: 배틀 HUD 카드·상태 텍스트의 투명도와 위치/크기를 미리보기로 보며 조정한다.
// 모든 Versus 배틀에 적용되며, 팝업이 닫힐 때 Mod 데이터를 저장한다.
class SettingsPopup final : public ::versus::DeferredPopup {
    // kHeight: 팝업 높이, kSideW: Battle Display 탭의 오른쪽 레이아웃 패널 너비, kGap: 두 패널 사이 간격
    static constexpr float kHeight = 290.f, kSideW = 132.f, kGap = 8.f;
    static constexpr float kTabsY = kHeight - 46.f;
    // 레이아웃 패널이 편집하는 대상 이름(내 카드 / 상대 카드 / 상태 텍스트)
    static constexpr char const* kPartNames[3] = {"My status", "Opponent", "Status text"};
    float m_width = 330.f;
    bool m_ranked = false;
    int m_tab = 0;
    // 현재 레이아웃 편집 대상(kPartOwn 등)
    int m_part = kPartOwn;
    CCMenuItemSpriteExtra* m_soundTab = nullptr;
    CCMenuItemSpriteExtra* m_displayTab = nullptr;
    CCNode* m_soundPage = nullptr;
    CCNode* m_displayPage = nullptr;
    CCNode* m_side = nullptr;
    Slider* m_music = nullptr;
    Slider* m_sfx = nullptr;
    Slider* m_rankedMusic = nullptr;
    CCLabelBMFont* m_musicValue = nullptr;
    CCLabelBMFont* m_sfxValue = nullptr;
    CCLabelBMFont* m_rankedValue = nullptr;
    CCNode* m_screen = nullptr;
    CCSize m_previewWindow;
    // 미리보기 HUD. 실제 배틀 HUD 를 축소해 보여 준다.
    Weak<BattleHUD> m_preview;
    CCLabelBMFont* m_textPreview = nullptr;
    // 미리보기에서 편집 중인 요소를 둘러싸는 사각형 표시
    CCDrawNode* m_mark = nullptr;
    Slider* m_own = nullptr;
    Slider* m_opponent = nullptr;
    Slider* m_text = nullptr;
    CCLabelBMFont* m_ownValue = nullptr;
    CCLabelBMFont* m_opponentValue = nullptr;
    CCLabelBMFont* m_textValue = nullptr;
    CCLabelBMFont* m_partName = nullptr;
    // 위치 기준점(Default, 네 모서리) 칩
    std::array<mapmode::Chip, 5> m_anchors;
    Slider* m_x = nullptr;
    Slider* m_y = nullptr;
    Slider* m_scale = nullptr;
    CCLabelBMFont* m_xValue = nullptr;
    CCLabelBMFont* m_yValue = nullptr;
    CCLabelBMFont* m_scaleValue = nullptr;

    // 슬라이더 값(0~1)과 퍼센트/오프셋/스케일 정수 사이의 변환 도우미.
    // 오프셋은 2단위, 스케일은 5단위로 맞춘다.
    static std::string percent(float value) {
        return fmt::format("{}%", static_cast<int>(std::lround(std::clamp(value, 0.f, 1.f) * 100.f)));
    }
    static int percentOf(Slider* slider) {
        return static_cast<int>(std::lround(std::clamp(slider->getValue(), 0.f, 1.f) * 100.f));
    }
    static float offsetValue(int offset) { return (offset + kPlacementOffsetMax) / (2.f * kPlacementOffsetMax); }
    static int offsetOf(Slider* slider) {
        int const raw =
            static_cast<int>(std::lround((std::clamp(slider->getValue(), 0.f, 1.f) * 2.f - 1.f) * kPlacementOffsetMax));
        return std::clamp(raw / 2 * 2, -kPlacementOffsetMax, kPlacementOffsetMax);
    }
    static float scaleValue(int scale) {
        return (scale - kPlacementScaleMin) / static_cast<float>(kPlacementScaleMax - kPlacementScaleMin);
    }
    static int scaleOf(Slider* slider) {
        int const raw =
            kPlacementScaleMin + static_cast<int>(std::lround(std::clamp(slider->getValue(), 0.f, 1.f) *
                                                              (kPlacementScaleMax - kPlacementScaleMin) / 5.f)) *
                                     5;
        return std::clamp(raw, kPlacementScaleMin, kPlacementScaleMax);
    }
    // 라벨 + 값 표시 + 슬라이더로 이루어진 한 행을 만든다. above 면 라벨을 슬라이더 위에 둔다.
    Slider* row(CCNode* page, char const* name, CCPoint at, float labelW, float sliderX, float sliderScale,
                float valueX, float value, SEL_MenuHandler handler, CCLabelBMFont*& text, bool above) {
        label(page, name, {at.x, above ? at.y + 17.f : at.y}, above ? .38f : .3f, labelW, kIce, true);
        text = label(page, "", {valueX, above ? at.y + 17.f : at.y}, above ? .36f : .3f, 52.f, ccWHITE);
        text->setAnchorPoint({1.f, .5f});
        auto* slider = Slider::create(this, handler, sliderScale);
        slider->setPosition({sliderX, at.y});
        slider->setValue(std::clamp(value, 0.f, 1.f));
        page->addChild(slider);
        return slider;
    }

    // 탭을 전환한다. Battle Display 탭에서는 오른쪽 패널을 위해 팝업을 왼쪽으로 옮기고,
    // 선택된 탭 버튼을 초록색으로 표시한다.
    void showTab(int tab) {
        m_tab = tab;
        s_lastTab = tab;
        m_soundPage->setVisible(tab == 0);
        m_displayPage->setVisible(tab == 1);
        m_side->setVisible(tab == 1);
        auto const window = CCDirector::sharedDirector()->getWinSize();
        m_mainLayer->setPositionX(window.width / 2.f - (tab == 1 ? (kGap + kSideW) / 2.f : 0.f));
        auto tint = [](CCMenuItemSpriteExtra* item, bool selected) {
            static_cast<ButtonSprite*>(item->getNormalImage())
                ->m_BGSprite->setColor(selected ? ccc3(100, 255, 125) : ccc3(155, 155, 155));
        };
        tint(m_soundTab, tab == 0);
        tint(m_displayTab, tab == 1);
    }
    void onSoundTab(CCObject*) { showTab(0); }
    void onDisplayTab(CCObject*) { showTab(1); }

    // 볼륨 표시를 현재 값으로 갱신한다.
    void refreshSound() {
        if (auto* manager = GameManager::sharedState()) {
            m_musicValue->setString(percent(manager->m_bgVolume).c_str());
            m_sfxValue->setString(percent(manager->m_sfxVolume).c_str());
        }
        if (m_rankedValue) m_rankedValue->setString(percent(audio::rankedMusicVolume()).c_str());
    }
    // 슬라이더 변경을 GD 설정과 FMOD 엔진에 바로 반영한다(음악 / 효과음).
    void onMusic(CCObject*) {
        float const value = std::clamp(m_music->getValue(), 0.f, 1.f);
        if (auto* manager = GameManager::sharedState()) manager->m_bgVolume = value;
        if (auto* engine = FMODAudioEngine::sharedEngine()) engine->setBackgroundMusicVolume(value);
        refreshSound();
    }
    void onSfx(CCObject*) {
        float const value = std::clamp(m_sfx->getValue(), 0.f, 1.f);
        if (auto* manager = GameManager::sharedState()) manager->m_sfxVolume = value;
        if (auto* engine = FMODAudioEngine::sharedEngine()) engine->setEffectsVolume(value);
        refreshSound();
    }
    // 랭크 로비 음악 볼륨 변경
    void onRankedMusic(CCObject*) {
        audio::setRankedMusicVolume(m_rankedMusic->getValue());
        refreshSound();
    }
    // Sound 탭 구성. 랭크 화면에서 열었을 때만 랭크 로비 음악 슬라이더가 추가된다.
    void buildSound() {
        m_soundPage = CCNode::create();
        m_mainLayer->addChild(m_soundPage);
        auto* manager = GameManager::sharedState();
        float const left = 26.f, right = m_width - 26.f, sliderX = m_width / 2.f;
        float const sliderScale = std::min(.8f, (m_width - 70.f) / 210.f);
        float y = kTabsY - 62.f;
        m_music = row(m_soundPage, "Music", {left, y}, 170.f, sliderX, sliderScale, right,
                      manager ? manager->m_bgVolume : 1.f, menu_selector(SettingsPopup::onMusic), m_musicValue, true);
        y -= 54.f;
        m_sfx = row(m_soundPage, "Sound Effects", {left, y}, 170.f, sliderX, sliderScale, right,
                    manager ? manager->m_sfxVolume : 1.f, menu_selector(SettingsPopup::onSfx), m_sfxValue, true);
        if (m_ranked) {
            y -= 54.f;
            m_rankedMusic =
                row(m_soundPage, "Ranked Lobby Music", {left, y}, 170.f, sliderX, sliderScale, right,
                    audio::rankedMusicVolume(), menu_selector(SettingsPopup::onRankedMusic), m_rankedValue, true);
        }
        label(m_soundPage, "Geometry Dash's own volume, saved with the game.", {m_width / 2.f, 26.f}, .55f,
              m_width - 40.f, kMuted, false, "chatFont.fnt");
        refreshSound();
    }

    // Battle Display 값 변경을 값 표시, 미리보기 HUD, 상태 텍스트, 선택 표시 사각형에 반영한다.
    void refreshDisplay() {
        m_ownValue->setString(fmt::format("{}%", battleCardOpacity(true)).c_str());
        m_opponentValue->setString(fmt::format("{}%", battleCardOpacity(false)).c_str());
        m_textValue->setString(fmt::format("{}%", battleTextOpacity()).c_str());
        auto const real = CCDirector::sharedDirector()->getWinSize();
        auto const window = m_previewWindow.width > 0.f ? m_previewWindow : real;
        if (auto hud = m_preview.lock()) {
            hud->setCardOpacity(battleCardAlpha(true), battleCardAlpha(false));
            hud->applyLayout();
        }
        if (m_textPreview) {
            auto const place = battleTextPlace(window);
            m_textPreview->setOpacity(battleTextAlpha());
            m_textPreview->setAnchorPoint(place.anchor);
            m_textPreview->setPosition(place.at);
            m_textPreview->setScale(place.scale);
        }
        if (m_mark) {
            m_mark->clear();
            CCRect box;
            if (m_part == kPartText && m_textPreview)
                box = m_textPreview->boundingBox();
            else {
                float const width = std::min(205.f, (real.width - 116.f) / 2.f);
                float const scale = battlePlacement(m_part).scale / 100.f;
                auto const origin = battleCardOrigin(m_part, window, width);
                box = CCRect(origin.x, origin.y, width * scale, 66.f * scale);
            }
            float const pad = 3.f;
            CCPoint corners[4] = {{box.getMinX() - pad, box.getMinY() - pad},
                                  {box.getMaxX() + pad, box.getMinY() - pad},
                                  {box.getMaxX() + pad, box.getMaxY() + pad},
                                  {box.getMinX() - pad, box.getMaxY() + pad}};
            m_mark->drawPolygon(corners, 4, {0.f, 0.f, 0.f, 0.f}, 2.f, {1.f, .85f, .3f, 1.f});
        }
    }
    // 투명도 슬라이더 콜백들과 Default(모두 100%로 되돌림)
    void onOwn(CCObject*) {
        setBattleCardOpacity(true, percentOf(m_own));
        refreshDisplay();
    }
    void onOpponent(CCObject*) {
        setBattleCardOpacity(false, percentOf(m_opponent));
        refreshDisplay();
    }
    void onText(CCObject*) {
        setBattleTextOpacity(percentOf(m_text));
        refreshDisplay();
    }
    void onDefault(CCObject*) {
        setBattleCardOpacity(true, 100);
        setBattleCardOpacity(false, 100);
        setBattleTextOpacity(100);
        m_own->setValue(1.f);
        m_opponent->setValue(1.f);
        m_text->setValue(1.f);
        refreshDisplay();
    }
    // Battle Display 탭 구성: 화면 비율을 유지한 미리보기 상자에 샘플 배틀 HUD 와
    // 상태 텍스트를 올리고, 아래에 투명도 슬라이더 3개를 둔다.
    void buildDisplay() {
        m_displayPage = CCNode::create();
        m_mainLayer->addChild(m_displayPage);
        auto const window = CCDirector::sharedDirector()->getWinSize();
        float const fitW = std::min({220.f, m_width - 40.f, 124.f * window.width / std::max(1.f, window.height)});
        float const scale = fitW / std::max(1.f, window.width);
        float const boxH = window.height * scale;
        float const boxW = std::max(fitW, m_width - 24.f);
        m_previewWindow = CCSize(boxW / std::max(.001f, scale), window.height);
        float const boxX = (m_width - boxW) / 2.f, boxTop = kTabsY - 16.f;
        auto* box = CCLayerGradient::create(ccc4(40, 92, 190, 255), ccc4(14, 32, 92, 255));
        box->setContentSize({boxW, boxH});
        box->setPosition({boxX, boxTop - boxH});
        m_displayPage->addChild(box);
        auto* ground = CCLayerColor::create(ccc4(0, 0, 0, 110), boxW, 22.f * scale);
        ground->setPosition({boxX, boxTop - boxH});
        m_displayPage->addChild(ground);
        label(m_displayPage, "PREVIEW", {boxX + boxW / 2.f, boxTop - boxH / 2.f}, .28f, 80.f, ccc3(150, 190, 255))
            ->setOpacity(150);
        m_screen = CCNode::create();
        m_screen->setContentSize(m_previewWindow);
        m_screen->setScale(scale);
        m_screen->setPosition({boxX, boxTop - boxH});
        m_displayPage->addChild(m_screen, 2);
        auto const& me = Service::get().profile();
        PlayerProfile opponent;
        opponent.name = "Opponent";
        opponent.icon = 37;
        opponent.color1 = 0xFF5A5A;
        opponent.color2 = 0xFFFFFF;
        GameRules rules;
        rules.mode = 0;
        rules.attempts = 3;
        rules.practice = false;
        if (auto* hud = BattleHUD::create(me, opponent, rules)) {
            BattlePlayerState mine, theirs;
            mine.currentPercent = 42;
            mine.bestPercent = 63;
            mine.inAttempt = true;
            theirs.currentPercent = 57;
            theirs.bestPercent = 57;
            theirs.inAttempt = true;
            hud->updatePlayers(mine, theirs);
            hud->setContentSize(m_previewWindow);
            hud->applyLayout();
            m_screen->addChild(hud);
            rebind(m_preview, hud);
        }
        m_textPreview = createCosmeticLabel("Opponent paused", "chatFont.fnt");
        m_screen->addChild(m_textPreview, 2);
        m_mark = CCDrawNode::create();
        m_screen->addChild(m_mark, 3);
        float const labelW = std::min(104.f, m_width * .3f), sliderX = m_width * .6f;
        float const sliderScale = std::min(.55f, (m_width * .38f) / 210.f), valueX = m_width - 16.f;
        float y = boxTop - boxH - 16.f;
        m_own = row(m_displayPage, "My status", {18.f, y}, labelW, sliderX, sliderScale, valueX,
                    battleCardOpacity(true) / 100.f, menu_selector(SettingsPopup::onOwn), m_ownValue, false);
        y -= 22.f;
        m_opponent =
            row(m_displayPage, "Opponent status", {18.f, y}, labelW, sliderX, sliderScale, valueX,
                battleCardOpacity(false) / 100.f, menu_selector(SettingsPopup::onOpponent), m_opponentValue, false);
        y -= 22.f;
        m_text = row(m_displayPage, "Status text", {18.f, y}, labelW, sliderX, sliderScale, valueX,
                     battleTextOpacity() / 100.f, menu_selector(SettingsPopup::onText), m_textValue, false);
        auto* buttons = menu(m_displayPage);
        mapmode::bigButton(buttons, this, menu_selector(SettingsPopup::onDefault), "Default", {m_width / 2.f, 21.f},
                           .42f);
        label(m_displayPage, "All Versus battles", {m_width - 16.f, 21.f}, .5f, 90.f, kIce, false, "chatFont.fnt")
            ->setAnchorPoint({1.f, .5f});
    }

    // 선택한 요소의 위치 설정을 레이아웃 패널(이름, 기준점, X/Y/Scale 값, 슬라이더)에 반영한다.
    // sliders 가 false 면 슬라이더 자체는 건드리지 않는다(드래그 중 되튐 방지).
    void refreshLayout(bool sliders = true) {
        auto const place = battlePlacement(m_part);
        m_partName->setString(kPartNames[m_part]);
        m_partName->limitLabelWidth(kSideW - 54.f, .34f, .1f);
        for (int anchor = 0; anchor < 5; ++anchor) m_anchors[anchor].paint(place.anchor == anchor);
        m_xValue->setString(fmt::format("{:+}", place.x).c_str());
        m_yValue->setString(fmt::format("{:+}", place.y).c_str());
        m_scaleValue->setString(fmt::format("{}%", place.scale).c_str());
        if (!sliders) return;
        m_x->setValue(offsetValue(place.x));
        m_y->setValue(offsetValue(place.y));
        m_scale->setValue(scaleValue(place.scale));
    }
    // 위치 설정을 저장하고 패널과 미리보기를 갱신한다.
    void changed(BattlePlacement const& place, bool sliders = true) {
        setBattlePlacement(m_part, place);
        refreshLayout(sliders);
        refreshDisplay();
    }
    // 편집 대상 전환 / 기준점 선택(오프셋은 0으로 초기화) / X, Y, Scale 슬라이더 / Reset
    void onPartBack(CCObject*) {
        m_part = (m_part + 2) % 3;
        refreshLayout();
        refreshDisplay();
    }
    void onPartNext(CCObject*) {
        m_part = (m_part + 1) % 3;
        refreshLayout();
        refreshDisplay();
    }
    void onAnchor(CCObject* sender) {
        auto place = battlePlacement(m_part);
        place.anchor = std::clamp(sender->getTag(), 0, 4);
        place.x = place.y = 0;
        changed(place);
    }
    void onOffsetX(CCObject*) {
        auto place = battlePlacement(m_part);
        place.x = offsetOf(m_x);
        changed(place, false);
    }
    void onOffsetY(CCObject*) {
        auto place = battlePlacement(m_part);
        place.y = offsetOf(m_y);
        changed(place, false);
    }
    void onScale(CCObject*) {
        auto place = battlePlacement(m_part);
        place.scale = scaleOf(m_scale);
        changed(place, false);
    }
    void onLayoutReset(CCObject*) { changed(BattlePlacement{}); }
    // 오른쪽 레이아웃 패널 구성: 편집 대상 선택, 기준점 칩 5개, X/Y/Scale 슬라이더, Reset 버튼
    void buildSide() {
        m_side = CCNode::create();
        m_side->setContentSize({kSideW, kHeight});
        m_side->setPosition({m_width + kGap, 0.f});
        m_side->setID("battle-display-layout"_spr);
        m_mainLayer->addChild(m_side);
        auto* back = NineSlice::create("GJ_square02.png");
        back->setContentSize({kSideW, kHeight});
        back->setPosition({kSideW / 2.f, kHeight / 2.f});
        m_side->addChild(back, -1);
        auto* buttons = menu(m_side);
        label(m_side, "LAYOUT", {kSideW / 2.f, kHeight - 20.f}, .46f, kSideW - 20.f);
        float const partY = kHeight - 46.f;
        mapmode::bigButton(buttons, this, menu_selector(SettingsPopup::onPartBack), "<", {17.f, partY}, .38f);
        mapmode::bigButton(buttons, this, menu_selector(SettingsPopup::onPartNext), ">", {kSideW - 17.f, partY}, .38f);
        m_partName = label(m_side, kPartNames[0], {kSideW / 2.f, partY}, .34f, kSideW - 54.f, ccc3(255, 225, 120));
        label(m_side, "POSITION", {kSideW / 2.f, partY - 21.f}, .24f, kSideW - 20.f, kIce);
        constexpr char const* names[5] = {"Default", "Top L", "Top R", "Bottom L", "Bottom R"};
        float const chipW = (kSideW - 30.f) / 2.f, chipH = 19.f;
        CCPoint const spots[5] = {{kSideW / 2.f, partY - 85.f},
                                  {13.f + chipW / 2.f, partY - 39.f},
                                  {kSideW - 13.f - chipW / 2.f, partY - 39.f},
                                  {13.f + chipW / 2.f, partY - 62.f},
                                  {kSideW - 13.f - chipW / 2.f, partY - 62.f}};
        for (int anchor = 0; anchor < 5; ++anchor) {
            m_anchors[anchor] = mapmode::chip(buttons, this, menu_selector(SettingsPopup::onAnchor), names[anchor],
                                              nullptr, anchor == 0 ? kSideW - 26.f : chipW, chipH, spots[anchor], .3f);
            m_anchors[anchor].item->setTag(anchor);
        }
        float const sliderScale = (kSideW - 30.f) / 210.f;
        auto offsetRow = [&](char const* name, float y, SEL_MenuHandler handler, CCLabelBMFont*& text) {
            label(m_side, name, {14.f, y + 13.f}, .3f, 60.f, kIce, true);
            text = label(m_side, "", {kSideW - 14.f, y + 13.f}, .3f, 50.f, ccWHITE);
            text->setAnchorPoint({1.f, .5f});
            auto* slider = Slider::create(this, handler, sliderScale);
            slider->setPosition({kSideW / 2.f, y});
            m_side->addChild(slider);
            return slider;
        };
        float y = partY - 122.f;
        m_x = offsetRow("X", y, menu_selector(SettingsPopup::onOffsetX), m_xValue);
        y -= 36.f;
        m_y = offsetRow("Y", y, menu_selector(SettingsPopup::onOffsetY), m_yValue);
        y -= 36.f;
        m_scale = offsetRow("Scale", y, menu_selector(SettingsPopup::onScale), m_scaleValue);
        mapmode::bigButton(buttons, this, menu_selector(SettingsPopup::onLayoutReset), "Reset", {kSideW / 2.f, 21.f},
                           .42f);
    }

    // 팝업 초기화. 화면 너비에 맞춰 폭을 정하고, 탭 버튼이 폭을 넘지 않게 축소해 배치한다.
    bool init(bool ranked, int tab) {
        auto const window = CCDirector::sharedDirector()->getWinSize();
        m_width = std::max(250.f, std::min(330.f, window.width - 24.f - kSideW - kGap));
        m_ranked = ranked;
        if (!Popup::init(m_width, kHeight, "GJ_square02.png")) return false;
        setTitle("Settings", "bigFont.fnt", .55f);
        m_soundTab = mapmode::bigButton(m_buttonMenu, this, menu_selector(SettingsPopup::onSoundTab), "Sound",
                                        {m_width / 2.f - 66.f, kTabsY}, .5f, "GJ_button_01.png");
        m_displayTab = mapmode::bigButton(m_buttonMenu, this, menu_selector(SettingsPopup::onDisplayTab),
                                          "Battle Display", {m_width / 2.f + 46.f, kTabsY}, .5f, "GJ_button_01.png");
        m_soundTab->setID("settings-sound-tab"_spr);
        m_displayTab->setID("versus-battle-display"_spr);
        float const tabsW = m_soundTab->getContentSize().width + 8.f + m_displayTab->getContentSize().width;
        float const fit = std::min(1.f, (m_width - 70.f) / std::max(1.f, tabsW));
        for (auto* item : {m_soundTab, m_displayTab}) {
            item->setScale(fit);
            item->m_baseScale = fit;
        }
        float const left = m_width / 2.f - tabsW * fit / 2.f;
        m_soundTab->setPositionX(left + m_soundTab->getContentSize().width * fit / 2.f);
        m_displayTab->setPositionX(left + tabsW * fit - m_displayTab->getContentSize().width * fit / 2.f);
        buildSound();
        buildDisplay();
        buildSide();
        refreshDisplay();
        refreshLayout();
        showTab(tab);
        return true;
    }
    // 팝업이 사라질 때 변경한 설정을 디스크에 저장한다.
    void onExit() override {
        if (auto* mod = Mod::get()) (void)mod->saveData();
        Popup::onExit();
    }
    // 마지막으로 연 탭을 기억해 다음에 같은 탭으로 연다.
    static inline int s_lastTab = 0;

public:
    static SettingsPopup* create(bool ranked, int tab = -1) {
        auto* result = new SettingsPopup;
        if (result->init(ranked, tab < 0 ? s_lastTab : std::clamp(tab, 0, 1))) {
            result->autorelease();
            return result;
        }
        delete result;
        return nullptr;
    }
};
// 설정 팝업을 연다. ranked 면 랭크 로비 음악 슬라이더를 포함한다.
void showSettings(bool ranked) {
    if (auto* popup = SettingsPopup::create(ranked)) popup->show();
}
