// 꾸미기 팝업의 오라 카드 목록: 드래그와 마우스 휠로 가로 스크롤하는 CCMenu.
// 카테고리 필터에 맞는 카드만 배치하고, 화면 밖 카드의 미리보기 애니메이션은 멈춘다.
class AuraCarousel : public CCMenu {
    // m_step은 카드 간격, m_offset은 현재 가로 스크롤 거리.
    std::vector<CCMenuItemSpriteExtra*> m_cards;
    float m_offset = 0.f;
    float m_step = 94.f;
    CCPoint m_start{};
    float m_startOffset = 0.f;
    int m_touchID = -1;
    bool m_dragging = false, m_pressedCard = false;
    bool m_filterChanged = false;
    AuraCategory m_category = AuraCategory::All;
    using Filter = bool (*)(int, AuraCategory);
    Filter m_filter = auraInCategory;

    // 월드 좌표가 이 메뉴 영역 안인지 확인한다.
    bool contains(CCPoint world) {
        auto const p = convertToNodeSpace(world);
        auto const s = getContentSize();
        return p.x >= 0.f && p.x <= s.width && p.y >= 0.f && p.y <= s.height;
    }
    // 필터를 통과한 카드를 순서대로 배치하고, 화면에 보이는 카드만 표시/애니메이션을 켠다.
    void positionCards() {
        auto const size = getContentSize();
        int index = 0;
        for (auto* card : m_cards) {
            bool const included = m_filter(card->getTag(), m_category);
            float const x = 4.f + index * m_step - m_offset;
            if (included) ++index;
            card->setPosition({x + 43.f, size.height / 2.f});
            bool const visible = included && x + 86.f > 0.f && x < size.width;
            if (visible != card->isVisible()) {
                card->setVisible(visible);
                setPreviewRunning(card, visible);
            }
        }
    }
    // 노드와 자식의 scheduler/액션을 재귀적으로 재개 또는 일시정지한다.
    static void setPreviewRunning(CCNode* node, bool running) {
        if (running)
            node->resumeSchedulerAndActions();
        else
            node->pauseSchedulerAndActions();
        for (auto* child : CCArrayExt<CCNode*>(node->getChildren())) setPreviewRunning(child, running);
    }
    // 스크롤 거리를 0부터 (전체 카드 길이 - 보이는 폭) 사이로 제한해 적용한다.
    void setOffset(float value) {
        auto const count = std::count_if(m_cards.begin(), m_cards.end(),
                                         [this](auto* card) { return m_filter(card->getTag(), m_category); });
        float const extent = std::max(0.f, count * m_step - getContentSize().width);
        m_offset = std::clamp(value, 0.f, extent);
        positionCards();
    }

public:
    static AuraCarousel* create(CCSize size) {
        auto* result = new AuraCarousel;
        if (!result->CCMenu::init()) {
            delete result;
            return nullptr;
        }
        result->autorelease();
        result->setContentSize(size);
        result->setAnchorPoint({0.f, 0.f});
        result->ignoreAnchorPointForPosition(false);
        result->setPosition({0.f, 0.f});
        result->setMouseEnabled(true);
        result->setID("cosmetics-aura-list"_spr);
        return result;
    }
    void addCard(CCMenuItemSpriteExtra* card) {
        addChild(card);
        m_cards.push_back(card);
        positionCards();
    }
    void scrollBy(float amount) { setOffset(m_offset + amount); }
    void setFilter(Filter filter) {
        m_filter = filter ? filter : auraInCategory;
        setOffset(0.f);
    }
    void setCategory(AuraCategory category) {
        m_category = category;
        m_filterChanged = m_touchID != -1;
        setOffset(0.f);
    }
    // 해당 오라 카드가 목록 가운데쯤에 오도록 스크롤한다.
    void reveal(int aura) {
        int index = 0;
        for (auto* card : m_cards) {
            if (!m_filter(card->getTag(), m_category)) continue;
            if (card->getTag() == aura) {
                setOffset(index * m_step + 47.f - getContentSize().width / 2.f);
                return;
            }
            ++index;
        }
    }
    void onEnter() override {
        CCMenu::onEnter();
        for (auto* card : m_cards) setPreviewRunning(card, card->isVisible());
    }
    // 터치 하나만 추적한다. 터치 시작 때 눌린 카드를 기억해 두고, 7px 이상 움직이면 드래그로
    // 보고 카드 누름은 취소한다.
    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        if (m_touchID != -1 || !nodeIsVisible(this) || !contains(touch->getLocation())) return false;
        m_touchID = touch->getID();
        m_start = convertToNodeSpace(touch->getLocation());
        m_startOffset = m_offset;
        m_dragging = false;
        m_filterChanged = false;
        m_pressedCard = CCMenu::ccTouchBegan(touch, event);
        return true;
    }
    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        if (touch->getID() != m_touchID || m_filterChanged) return;
        auto const p = convertToNodeSpace(touch->getLocation());
        if (!m_dragging && (std::abs(p.x - m_start.x) > 7.f || std::abs(p.y - m_start.y) > 7.f)) {
            m_dragging = true;
            if (m_pressedCard) CCMenu::ccTouchCancelled(touch, event);
            m_pressedCard = false;
        }
        if (m_dragging) setOffset(m_startOffset + m_start.x - p.x);
    }
    // 드래그가 아니고 영역 안에서 뗐을 때만 카드를 활성화한다.
    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        if (touch->getID() != m_touchID) return;
        bool const activate = m_pressedCard && !m_dragging && !m_filterChanged && contains(touch->getLocation());
        bool const cancel = m_pressedCard && !activate;
        m_touchID = -1;
        m_pressedCard = false;
        m_dragging = false;
        if (activate)
            CCMenu::ccTouchEnded(touch, event);
        else if (cancel)
            CCMenu::ccTouchCancelled(touch, event);
    }
    void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
        if (touch->getID() != m_touchID) return;
        if (m_pressedCard) CCMenu::ccTouchCancelled(touch, event);
        m_touchID = -1;
        m_pressedCard = false;
        m_dragging = false;
    }
    // 마우스 휠: 가로 입력이 있으면 그것을, 없으면 세로 입력을 가로 스크롤로 쓴다.
    void scrollWheel(float y, float x) override {
        if (!nodeIsVisible(this) || !contains(getMousePos())) return;
        scrollBy((std::abs(x) > .01f ? x : y) * 2.f);
    }
};
