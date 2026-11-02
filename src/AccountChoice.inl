// 계정 선택 팝업. 한 GD 계정에 Versus 기록이 여러 개 있을 때, 하나만 남기도록 고르게 한다.
// 선택하지 않은 기록은 폐기되므로 확인창을 한 번 더 거친다. 선택은 Service::chooseAccount 로 서버에 저장.
class AccountChoicePopup final : public ::versus::DeferredPopup {
    // 한 페이지에 보이는 기록 행 수
    static constexpr int kRows = 3;
    std::vector<AccountChoice> m_choices;
    std::function<void()> m_done, m_later;
    CCNode* m_rows = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_pageLabel = nullptr;
    CCMenuItemSpriteExtra* m_previous = nullptr;
    CCMenuItemSpriteExtra* m_next = nullptr;
    CCMenuItemSpriteExtra* m_laterButton = nullptr;
    std::vector<CCMenuItemSpriteExtra*> m_keep;
    int m_page = 0;
    bool m_pending = false, m_closing = false;

    // 레이팅 표시 문자열. 배치 판이 남았으면 "Placement n/m", 아니면 "R레이팅".
    static std::string ratingText(AccountChoice const& choice) {
        auto const& rank = choice.rank;
        if (rank.placementRequired > 0 && rank.placementCompleted < rank.placementRequired)
            return fmt::format("Placement {}/{}", rank.placementCompleted, rank.placementRequired);
        return fmt::format("R{:.0f}", rank.rating);
    }
    // 팝업 UI 구성. 닫기 버튼은 숨기고 "Later" 로만 미룰 수 있다.
    bool init(std::vector<AccountChoice> choices, std::function<void()> done, std::function<void()> later) {
        if (!Popup::init(420.f, 290.f, "GJ_square02.png")) return false;
        m_choices = std::move(choices);
        m_done = std::move(done);
        m_later = std::move(later);
        m_closeBtn->setVisible(false);
        auto* title = label(m_mainLayer, "IMPORTANT NOTICE", {210.f, 266.f}, .6f, 300.f, kAlert);
        title->setID("account-choice-title"_spr);
        label(m_mainLayer,
              fmt::format("Your Geometry Dash account {} has {} Versus records", Service::get().profile().name,
                          m_choices.size()),
              {210.f, 242.f}, .5f, 390.f, ccWHITE, false, "chatFont.fnt");
        label(m_mainLayer, "Versus now keeps ONE record per account. Choose the record to continue with.",
              {210.f, 226.f}, .5f, 390.f, kIce, false, "chatFont.fnt");
        m_rows = CCNode::create();
        m_mainLayer->addChild(m_rows);
        m_status = label(m_mainLayer, "", {210.f, 54.f}, .45f, 380.f, kIce, false, "chatFont.fnt");
        m_pageLabel = label(m_mainLayer, "1 / 1", {210.f, 26.f}, .33f, 70.f, kIce);
        m_previous =
            button(m_buttonMenu, this, menu_selector(AccountChoicePopup::onPrevious), "<", {160.f, 26.f}, .45f);
        m_next = button(m_buttonMenu, this, menu_selector(AccountChoicePopup::onNext), ">", {260.f, 26.f}, .45f);
        m_laterButton = button(m_buttonMenu, this, menu_selector(AccountChoicePopup::onLater), "Later", {50.f, 26.f},
                               .42f, "GJ_button_04.png");
        render();
        return true;
    }
    // 전체 페이지 수(최소 1)
    int pages() const { return std::max(1, (static_cast<int>(m_choices.size()) + kRows - 1) / kRows); }
    // 현재 페이지의 기록 행을 다시 그린다. 저장 중에는 버튼을 비활성화한다.
    void render() {
        m_rows->removeAllChildren();
        m_keep.clear();
        m_page = std::clamp(m_page, 0, pages() - 1);
        m_pageLabel->setString(fmt::format("{} / {}", m_page + 1, pages()).c_str());
        m_pageLabel->setVisible(pages() > 1);
        m_previous->setVisible(pages() > 1);
        m_next->setVisible(pages() > 1);
        enabled(m_previous, m_page > 0 && !m_pending);
        enabled(m_next, m_page + 1 < pages() && !m_pending);
        enabled(m_laterButton, !m_pending);
        auto* actions = menu(m_rows);
        auto const count = static_cast<int>(m_choices.size());
        for (int row = 0; row < kRows; ++row) {
            int const index = m_page * kRows + row;
            if (index >= count) break;
            auto const& choice = m_choices[index];
            auto const& rank = choice.rank;
            float const y = 186.f - row * 44.f;
            auto* cell = panel(m_rows, {14.f, y - 20.f}, {392.f, 40.f}, false);
            player(cell, rank.player, {22.f, 20.f}, .62f);
            styleNickname(label(cell, rank.player.name, {44.f, 28.f}, .36f, 130.f, ccWHITE, true),
                          rank.player.membership, rank.player.cosmetics);
            if (choice.thisDevice)
                label(cell, "THIS DEVICE", {44.f, 4.f}, .3f, 70.f, ccc3(130, 255, 165), true, "chatFont.fnt");
            label(cell,
                  fmt::format("{}W {}L  |  {} matches  |  {}", rank.wins, rank.losses, rank.matches,
                              choice.lastPlayedAt > 0 ? playedText(choice.lastPlayedAt) : std::string("never played")),
                  {44.f, choice.thisDevice ? 15.f : 12.f}, .4f, 180.f, kIce, true, "chatFont.fnt");
            rankedFace(cell, rank.tier, {240.f, 20.f}, .42f);
            rankedGradient(label(cell, ratingText(choice), {286.f, 24.f}, .38f, 70.f), rank.tier);
            label(cell, rank.globalRank > 0 ? fmt::format("#{}", rank.globalRank) : std::string("unranked"),
                  {286.f, 9.f}, .4f, 70.f, kMuted, false, "chatFont.fnt");
            auto* keep = button(actions, this, menu_selector(AccountChoicePopup::onKeep), "Keep", {370.f, y}, .5f);
            keep->setTag(index);
            enabled(keep, !m_pending);
            m_keep.push_back(keep);
        }
    }
    // 페이지 이동 및 "Later" 처리. 저장 중에는 동작하지 않는다.
    void onPrevious(CCObject*) {
        if (!m_pending && m_page > 0) {
            --m_page;
            render();
        }
    }
    void onNext(CCObject*) {
        if (!m_pending) {
            ++m_page;
            render();
        }
    }
    void onLater(CCObject*) {
        if (m_pending) return;
        auto later = m_later;
        finish();
        if (later) later();
    }
    // "Keep" 클릭 시 확인창을 띄운다. 나머지 기록은 영구 폐기됨을 경고한다.
    void onKeep(CCObject* sender) {
        int const index = sender->getTag();
        if (m_pending || index < 0 || index >= static_cast<int>(m_choices.size())) return;
        auto const& choice = m_choices[index];
        auto const others = m_choices.size() - 1;
        auto const body = fmt::format(
            "Keep <cy>{}</c> ({}, {} matches)?\n\n<cr>The other {} record{} will be discarded</c> and can never be "
            "played again. <cr>This cannot be undone.</c>",
            choice.rank.player.name, ratingText(choice), choice.rank.matches, others, others == 1 ? "" : "s");
        createQuickPopup(
            "Warning", body, "Back", "Keep", 360.f,
            [self = Weak<AccountChoicePopup>(this), uid = choice.uid, rating = ratingText(choice)](auto*, bool yes) {
                if (yes)
                    if (auto owner = self.lock()) owner->keep(uid, rating);
            });
    }
    // 선택한 uid 를 서버에 저장한다. 실패하면 오류를 상태줄에 보여 주고 다시 선택할 수 있게 한다.
    // 성공하면 알림을 띄우고 팝업을 닫은 뒤 done 콜백을 호출한다.
    void keep(std::string uid, std::string rating) {
        if (m_pending) return;
        m_pending = true;
        m_status->setString("Saving your choice...");
        render();
        Service::get().chooseAccount(
            std::move(uid), [self = Weak<AccountChoicePopup>(this), rating](bool ok, std::string error) {
                auto owner = self.lock();
                if (!owner) return;
                owner->m_pending = false;
                if (!ok) {
                    owner->m_status->setString(error.c_str());
                    owner->m_status->limitLabelWidth(380.f, .45f, .2f);
                    owner->render();
                    return;
                }
                Notification::create(fmt::format("Versus record kept: {}", rating), NotificationIcon::Success, 2.5f)
                    ->show();
                auto done = owner->m_done;
                owner->finish();
                if (done) done();
            });
    }
    // 팝업을 실제로 닫는다. m_closing 을 먼저 세워 onClose 가 통과하게 한다.
    void finish() {
        m_closing = true;
        Popup::onClose(nullptr);
    }

public:
    // 사용자의 일반 닫기(뒤로가기 등)는 무시하고, finish() 로 닫을 때만 실제로 닫는다.
    void onClose(CCObject*) override {
        if (m_closing) Popup::onClose(nullptr);
    }
    static AccountChoicePopup* create(std::vector<AccountChoice> choices, std::function<void()> done,
                                      std::function<void()> later) {
        auto* result = new AccountChoicePopup;
        if (result->init(std::move(choices), std::move(done), std::move(later))) {
            result->autorelease();
            return result;
        }
        delete result;
        return nullptr;
    }
};
