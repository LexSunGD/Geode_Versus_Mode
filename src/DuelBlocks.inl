// 듀얼 요청 차단(Duel Blocks) UI 조각. VersusUI.cpp에 #include된다.
// 차단 기간 선택 팝업(DuelBlockLengthPopup)과, 차단 목록 / 요청 수신 설정을 다루는
// DuelRequestsPopup, 그리고 관련 문구 도우미를 담고 있다.
// 차단 기간 선택지 (표시 이름, 분 단위). 0분은 영구 차단(Forever)이다.
constexpr std::array<std::pair<char const*, int>, 8> kDuelBlockLengths = {{{"10 min", 10},
                                                                           {"30 min", 30},
                                                                           {"1 hour", 60},
                                                                           {"6 hours", 360},
                                                                           {"1 day", 1'440},
                                                                           {"7 days", 10'080},
                                                                           {"30 days", 43'200},
                                                                           {"Forever", 0}}};
// 기본으로 선택되는 기간의 인덱스 (1 hour).
constexpr int kDuelBlockDefault = 2;

// 차단 기간(분)을 "for 1 hour" 같은 문구로 바꾼다. 선택지에 없으면 시간 길이로 표기.
std::string duelBlockLengthText(int minutes) {
    if (minutes <= 0) return "permanently";
    for (auto const& [name, value] : kDuelBlockLengths)
        if (value == minutes) return fmt::format("for {}", name);
    return "for " + spanText(static_cast<int64_t>(minutes) * 60'000);
}
// 목록에 보여 줄 남은 차단 시간 문구 (영구면 Permanent). 서버 시각 기준으로 계산한다.
std::string duelBlockEndText(DuelBlock const& block) {
    if (block.until <= 0) return "Permanent";
    return spanText(block.until - Service::get().serverNow()) + " left";
}
// 차단 대상 이름. 익명이거나 프로필이 없으면 Anonymous player.
std::string duelBlockName(DuelBlock const& block) {
    if (block.anonymous || !block.player) return "Anonymous player";
    return block.player->name.empty() ? std::string("Player") : block.player->name;
}
// 익명 발신자용 회색 그림자 아이콘 (물음표 표시).
void duelShadowIcon(CCNode* parent, CCPoint at, float scale) {
    PlayerProfile shadow;
    shadow.name = "Anonymous";
    shadow.color1 = 0x3C3C4A;
    shadow.color2 = 0x707084;
    player(parent, shadow, at, scale);
    label(parent, "?", at, .6f * scale, 30.f * scale, ccWHITE, false, "bigFont.fnt")->setZOrder(5);
}
// 내가 받는 듀얼 요청 조건 요약 문구: 꺼져 있으면 안내, 켜져 있으면
// 레이팅 하한(내 레이팅 - gap 이상)과 익명 허용 여부.
std::string duelLimitText(DuelSettings const& settings, std::optional<int> rating, int gap) {
    if (!settings.enabled) return "Duel requests are OFF - nobody can send you one.";
    auto const range = rating ? fmt::format("R{} and up", std::max(0, *rating - gap + 1))
                              : fmt::format("rated less than {} below you", gap);
    return fmt::format("Accepting: {}  |  {}", range, settings.anonymous ? "anonymous OK" : "no anonymous");
}

// 차단 기간을 고르는 팝업. 칩 8개 중 하나를 선택하고 확인하면 chosen(분)을 호출한다.
// 선택한 기간의 종료 시각(서버 시각 기준 현지 표기)을 아래에 보여 준다.
class DuelBlockLengthPopup final : public ::versus::DeferredPopup {
    std::function<void(int)> m_chosen;
    int m_choice = kDuelBlockDefault;
    std::array<CCMenuItemSpriteExtra*, kDuelBlockLengths.size()> m_chips{};
    CCLabelBMFont* m_ends = nullptr;
    bool init(std::string const& title, std::string const& who, std::string const& action, int choice,
              std::function<void(int)> chosen) {
        if (!Popup::init(330.f, 226.f, "GJ_square02.png")) return false;
        m_chosen = std::move(chosen);
        m_choice = std::clamp(choice, 0, static_cast<int>(kDuelBlockLengths.size()) - 1);
        setTitle(title.c_str(), "goldFont.fnt", .7f);
        label(m_mainLayer, who, {165.f, 182.f}, .42f, 290.f, ccc3(255, 230, 120), false, "bigFont.fnt");
        label(m_mainLayer, "Block duel requests from this player for:", {165.f, 162.f}, .5f, 300.f, kIce, false,
              "chatFont.fnt");
        for (size_t i = 0; i < kDuelBlockLengths.size(); ++i) {
            auto* chip = button(m_buttonMenu, this, menu_selector(DuelBlockLengthPopup::onChoice), "",
                                {48.f + static_cast<float>(i % 4) * 78.f, 134.f - static_cast<float>(i / 4) * 32.f});
            chip->setTag(static_cast<int>(i));
            chip->setID(fmt::format("duel-block-length-{}", i));
            m_chips[i] = chip;
        }
        m_ends = label(m_mainLayer, "", {165.f, 72.f}, .45f, 300.f, kMuted, false, "chatFont.fnt");
        button(m_buttonMenu, this, menu_selector(DuelBlockLengthPopup::onCancel), "Cancel", {95.f, 32.f}, .6f,
               "GJ_button_06.png");
        button(m_buttonMenu, this, menu_selector(DuelBlockLengthPopup::onConfirm), action, {235.f, 32.f}, .6f,
               "GJ_button_01.png")
            ->setID("duel-block-confirm"_spr);
        refresh();
        return true;
    }
    void refresh() {
        for (size_t i = 0; i < m_chips.size(); ++i)
            paintChoice(m_chips[i], kDuelBlockLengths[i].first, static_cast<int>(i) == m_choice, 70.f);
        int const minutes = kDuelBlockLengths[m_choice].second;
        m_ends->setString(minutes ? fmt::format("Until {}", localDateTime(Service::get().serverNow() +
                                                                          static_cast<int64_t>(minutes) * 60'000))
                                        .c_str()
                                  : "Until you unblock them (Duel Requests window)");
        m_ends->setColor(minutes ? kMuted : ccc3(255, 175, 90));
        m_ends->limitLabelWidth(300.f, .45f, .2f);
    }
    void onChoice(CCObject* sender) {
        m_choice = std::clamp(sender->getTag(), 0, static_cast<int>(kDuelBlockLengths.size()) - 1);
        refresh();
    }
    void onCancel(CCObject*) { Popup::onClose(nullptr); }
    void onConfirm(CCObject*) {
        auto chosen = std::move(m_chosen);
        int const minutes = kDuelBlockLengths[m_choice].second;
        Popup::onClose(nullptr);
        if (chosen) chosen(minutes);
    }

public:
    // 기존 차단의 남은 시간에 맞는 선택지 인덱스 (남은 시간 이상인 가장 짧은 기간, 영구면 Forever).
    static int choiceFor(DuelBlock const& block) {
        if (block.until <= 0) return static_cast<int>(kDuelBlockLengths.size()) - 1;
        auto const left = block.until - Service::get().serverNow();
        for (size_t i = 0; i + 1 < kDuelBlockLengths.size(); ++i)
            if (static_cast<int64_t>(kDuelBlockLengths[i].second) * 60'000 >= left) return static_cast<int>(i);
        return static_cast<int>(kDuelBlockLengths.size()) - 2;
    }
    static DuelBlockLengthPopup* create(std::string const& title, std::string const& who, std::string const& action,
                                        int choice, std::function<void(int)> chosen) {
        auto* result = new DuelBlockLengthPopup;
        if (result->init(title, who, action, choice, std::move(chosen))) {
            result->autorelease();
            return result;
        }
        delete result;
        return nullptr;
    }
};

// Duel Requests / Duel Blocks 팝업. 차단된 플레이어 목록(페이지 4개씩, 수정 / 해제 / 전체 해제)과
// 요청 수신 설정(켜기 / 익명 허용) 탭으로 이루어진다. season2이면 Blocked 탭만 쓴다.
// 서버 요청은 m_busy로 한 번에 하나만 보내고, 응답 후 목록을 갱신한다.
class DuelRequestsPopup final : public ::versus::DeferredPopup {
    static constexpr int kPage = 4;
    enum Tab { kBlocked = 0, kSettings = 1 };
    std::function<void()> m_saved;
    int m_tab = kBlocked, m_page = 0, m_generation = 0;
    bool m_loaded = false, m_busy = false;
    std::string m_error;
    DuelBlockList m_list;
    std::array<CCMenuItemSpriteExtra*, 2> m_tabs{};
    CCNode* m_blocked = nullptr;
    CCNode* m_rows = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_pageLabel = nullptr;
    CCMenuItemSpriteExtra* m_previous = nullptr;
    CCMenuItemSpriteExtra* m_next = nullptr;
    CCMenuItemSpriteExtra* m_clear = nullptr;
    CCNode* m_settings = nullptr;
    CCMenuItemToggler* m_enabledToggle = nullptr;
    CCMenuItemToggler* m_anonymousToggle = nullptr;
    bool m_enabled = true, m_anonymous = true;
    CCLabelBMFont* m_rule = nullptr;
    CCLabelBMFont* m_summary = nullptr;
    CCLabelBMFont* m_note = nullptr;
    CCMenuItemSpriteExtra* m_save = nullptr;

    bool m_season2 = false;
    // 현재 토글 상태를 DuelSettings로 만든다.
    DuelSettings typed() const {
        DuelSettings settings;
        settings.enabled = m_enabled;
        settings.anonymous = m_anonymous;
        return settings;
    }
    // 탭, 차단 목록 영역(상태 문구, 페이지 이동, Unblock All), 설정 영역(토글 2개, 규칙 / 요약, Save)을 만든다.
    // 서버에서 목록을 받아 오고 15초마다 tick()으로 만료된 차단을 정리한다.
    bool init(std::function<void()> saved, bool season2) {
        if (!Popup::init(400.f, 270.f, "GJ_square02.png")) return false;
        m_saved = std::move(saved);
        m_season2 = season2;
        setTitle(season2 ? "Duel Blocks" : "Duel Requests", "goldFont.fnt", .72f);
        setID("duel-requests-popup"_spr);
        for (int i = 0; i < 2; ++i) {
            m_tabs[i] =
                button(m_buttonMenu, this, menu_selector(DuelRequestsPopup::onTab), "", {140.f + i * 120.f, 220.f});
            m_tabs[i]->setTag(i);
            m_tabs[i]->setID(i == kBlocked ? "duel-requests-blocked-tab" : "duel-requests-settings-tab");
        }
        m_blocked = CCNode::create();
        m_mainLayer->addChild(m_blocked);
        m_rows = CCNode::create();
        m_blocked->addChild(m_rows);
        m_status = label(m_blocked, "Loading...", {200.f, 130.f}, .5f, 360.f, kIce, false, "chatFont.fnt");
        auto* pager = menu(m_blocked);
        m_previous = button(pager, this, menu_selector(DuelRequestsPopup::onPrevious), "<", {150.f, 26.f}, .45f);
        m_next = button(pager, this, menu_selector(DuelRequestsPopup::onNext), ">", {250.f, 26.f}, .45f);
        m_pageLabel = label(m_blocked, "1 / 1", {200.f, 26.f}, .33f, 70.f, kIce);
        m_clear = button(pager, this, menu_selector(DuelRequestsPopup::onUnblockAll), "Unblock All", {336.f, 26.f},
                         .45f, "GJ_button_06.png");
        m_clear->setID("duel-requests-unblock-all"_spr);
        m_settings = CCNode::create();
        m_mainLayer->addChild(m_settings);
        auto* options = menu(m_settings);
        m_enabledToggle =
            CCMenuItemToggler::createWithStandardSprites(this, menu_selector(DuelRequestsPopup::onEnabledToggle), .6f);
        m_enabledToggle->setPosition({36.f, 190.f});
        options->addChild(m_enabledToggle);
        m_enabledToggle->setID("duel-requests-enabled-toggle"_spr);
        label(m_settings, "Receive duel requests", {54.f, 194.f}, .36f, 300.f, ccWHITE, true);
        label(m_settings, "Off: nobody can send you a duel request", {54.f, 181.f}, .42f, 300.f, kMuted, true,
              "chatFont.fnt");
        m_anonymousToggle = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(DuelRequestsPopup::onAnonymousToggle), .6f);
        m_anonymousToggle->setPosition({36.f, 156.f});
        options->addChild(m_anonymousToggle);
        m_anonymousToggle->setID("duel-requests-anonymous-toggle"_spr);
        label(m_settings, "Allow anonymous requests", {54.f, 160.f}, .36f, 300.f, ccWHITE, true);
        label(m_settings, "Senders who hide their name until you accept", {54.f, 147.f}, .42f, 300.f, kMuted, true,
              "chatFont.fnt");
        label(m_settings, "Rating rule", {22.f, 122.f}, .34f, 170.f, ccWHITE, true);
        m_rule = label(m_settings, "", {22.f, 108.f}, .42f, 356.f, kMuted, true, "chatFont.fnt");
        m_summary = label(m_settings, "", {200.f, 76.f}, .48f, 370.f, ccc3(255, 221, 120), false, "chatFont.fnt");
        m_note = label(m_settings, "Players you blocked are refused either way.", {200.f, 56.f}, .4f, 370.f, kMuted,
                       false, "chatFont.fnt");
        m_save = button(options, this, menu_selector(DuelRequestsPopup::onSave), "Save", {200.f, 24.f}, .6f);
        m_save->setID("duel-requests-save"_spr);
        load();
        schedule(schedule_selector(DuelRequestsPopup::tick), 15.f);
        return true;
    }
    // 서버에서 차단 목록과 설정을 새로 받는다. generation으로 오래된 응답은 버린다.
    void load() {
        int const generation = ++m_generation;
        m_loaded = false;
        m_error.clear();
        render();
        Service::get().fetchDuelBlocks(
            [self = Weak<DuelRequestsPopup>(this), generation](std::optional<DuelBlockList> list, std::string error) {
                auto owner = self.lock();
                if (!owner || owner->m_generation != generation) return;
                owner->adopt(std::move(list), std::move(error), true);
            });
    }
    // 받은 목록을 반영한다. fields가 true면 설정 토글도 서버 값으로 맞춘다. 실패하면 오류 문구 표시.
    void adopt(std::optional<DuelBlockList> list, std::string error, bool fields) {
        m_loaded = true;
        if (!list) {
            m_error = error.empty() ? std::string("Could not load. Try again.") : std::move(error);
            render();
            return;
        }
        m_error.clear();
        m_list = std::move(*list);
        if (fields) {
            m_enabled = m_list.settings.enabled;
            m_anonymous = m_list.settings.anonymous;
            m_enabledToggle->toggle(m_enabled);
            m_anonymousToggle->toggle(m_anonymous);
        }
        render();
    }
    void busy(bool value) {
        m_busy = value;
        render();
    }
    // 상태에 맞게 탭 / 버튼 활성화와 두 영역의 표시를 갱신한다 (로딩 중이거나 요청 중이면 입력 막음).
    void render() {
        paintChoice(m_tabs[kBlocked],
                    m_loaded && m_error.empty() ? fmt::format("Blocked ({})", m_list.blocks.size()).c_str() : "Blocked",
                    m_tab == kBlocked, 100.f);
        paintChoice(m_tabs[kSettings], "Settings", m_tab == kSettings, 100.f);
        if (m_season2) {
            m_tab = kBlocked;
            for (auto* tab : m_tabs) tab->setVisible(false);
        }
        m_blocked->setVisible(m_tab == kBlocked);
        m_settings->setVisible(m_tab == kSettings);
        enabled(m_save, m_loaded && m_error.empty() && !m_busy);
        m_enabledToggle->setEnabled(m_loaded && !m_busy);
        m_anonymousToggle->setEnabled(m_loaded && !m_busy);
        updateSummary();
        renderBlocked();
    }
    // 현재 페이지의 차단 행을 다시 그린다 (이름, 남은 시간, 차단된 시점, Edit / Unblock 버튼).
    // 목록이 비었거나 오류 / 로딩 중이면 상태 문구만 보여 준다.
    void renderBlocked() {
        m_rows->removeAllChildrenWithCleanup(true);
        int const count = m_loaded && m_error.empty() ? static_cast<int>(m_list.blocks.size()) : 0;
        int const pages = std::max(1, (count + kPage - 1) / kPage);
        m_page = std::clamp(m_page, 0, pages - 1);
        m_pageLabel->setString(fmt::format("{} / {}", m_page + 1, pages).c_str());
        enabled(m_previous, m_page > 0);
        enabled(m_next, m_page + 1 < pages);
        m_clear->setVisible(count > 0);
        enabled(m_clear, !m_busy);
        m_status->setVisible(!m_loaded || !m_error.empty() || count == 0);
        m_status->setString(!m_loaded          ? "Loading..."
                            : !m_error.empty() ? m_error.c_str()
                            : m_season2        ? "No blocked players. Tap Block on a duel request to block its sender."
                                        : "No blocked players. Tap Ignore on a duel request to block its sender.");
        m_status->setColor(m_error.empty() ? kIce : ccc3(255, 104, 92));
        m_status->limitLabelWidth(360.f, .5f, .2f);
        if (count == 0) return;
        auto* actions = menu(m_rows);
        auto const now = Service::get().serverNow();
        for (int row = 0; row < kPage; ++row) {
            int const index = m_page * kPage + row;
            if (index >= count) break;
            auto const& block = m_list.blocks[index];
            float const y = 186.f - row * 36.f;
            auto* cell = panel(m_rows, {14.f, y - 15.f}, {372.f, 30.f}, false);
            if (block.anonymous || !block.player)
                duelShadowIcon(cell, {17.f, 15.f}, .5f);
            else
                player(cell, *block.player, {17.f, 15.f}, .5f);
            auto* name = label(cell, duelBlockName(block), {36.f, 21.f}, .36f, 190.f,
                               block.anonymous ? ccc3(200, 200, 215) : ccWHITE, true);
            if (block.player && !block.anonymous)
                styleNickname(name, block.player->membership, block.player->cosmetics);
            label(cell, fmt::format("{}  |  blocked {}", duelBlockEndText(block), agoText(now - block.createdAt)),
                  {36.f, 8.f}, .4f, 220.f, block.until <= 0 ? ccc3(255, 175, 90) : kIce, true, "chatFont.fnt");
            auto* edit = button(actions, this, menu_selector(DuelRequestsPopup::onEdit), "Edit", {292.f, y}, .42f);
            edit->setTag(index);
            enabled(edit, !m_busy);
            auto* lift = button(actions, this, menu_selector(DuelRequestsPopup::onUnblock), "Unblock", {350.f, y}, .42f,
                                "GJ_button_06.png");
            lift->setTag(index);
            enabled(lift, !m_busy);
        }
    }
    // 설정 탭의 규칙 문구와 요약(현재 토글 기준)을 갱신한다.
    void updateSummary() {
        if (!m_summary) return;
        auto const settings = typed();
        m_rule->setString(
            fmt::format("Players rated {} or more below you can't send you duel requests.", m_list.gap).c_str());
        m_rule->limitLabelWidth(356.f, .42f, .2f);
        m_summary->setString(m_loaded ? duelLimitText(settings, m_list.rating, m_list.gap).c_str() : "Loading...");
        m_summary->setColor(settings.enabled ? ccc3(255, 221, 120) : ccc3(255, 104, 92));
        m_summary->limitLabelWidth(370.f, .48f, .2f);
    }
    void say(std::string const& text, ccColor3B color) {
        m_note->setString(text.c_str());
        m_note->setColor(color);
        m_note->limitLabelWidth(370.f, .4f, .2f);
    }
    // 서버 응답 콜백을 만든다. 실패하면 오류 알림(점검 공지 우선), 성공하면 목록을 반영하고 성공 알림을 띄운다.
    auto answer(std::string success, bool fields) {
        return [self = Weak<DuelRequestsPopup>(this), success = std::move(success),
                fields](std::optional<DuelBlockList> list, std::string error) {
            auto owner = self.lock();
            if (!owner) return;
            owner->m_busy = false;
            if (!list) {
                owner->render();
                if (!showMaintenanceNotice(error))
                    FLAlertLayer::create(
                        "Duel Requests",
                        (error.empty() ? std::string("Could not save. Try again.") : plainAlertText(error)).c_str(),
                        "OK")
                        ->show();
                return;
            }
            owner->adopt(std::move(list), {}, fields);
            if (!success.empty()) Notification::create(success, NotificationIcon::Success, 2.f)->show();
        };
    }
    // 주기 호출: 기간이 끝난 차단을 서버 시각 기준으로 목록에서 제거하고 다시 그린다.
    void tick(float) {
        if (!m_loaded || m_busy) return;
        auto const now = Service::get().serverNow();
        auto& blocks = m_list.blocks;
        auto const before = blocks.size();
        blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                    [now](DuelBlock const& block) { return block.until > 0 && block.until <= now; }),
                     blocks.end());
        if (m_tab == kBlocked || blocks.size() != before) render();
    }
    DuelBlock const* blockAt(CCObject* sender) const {
        int const index = sender ? sender->getTag() : -1;
        return index >= 0 && index < static_cast<int>(m_list.blocks.size()) ? &m_list.blocks[index] : nullptr;
    }
    // 버튼 콜백. 탭 전환 / 페이지 이동, Edit(기간 변경), Unblock(확인 후 해제), Unblock All(확인 후 전체 해제).
    void onTab(CCObject* sender) {
        int const tab = std::clamp(sender->getTag(), 0, 1);
        if (tab == m_tab) return;
        m_tab = tab;
        render();
    }
    void onPrevious(CCObject*) {
        if (m_page > 0) {
            --m_page;
            render();
        }
    }
    void onNext(CCObject*) {
        ++m_page;
        render();
    }
    void onEdit(CCObject* sender) {
        auto const* block = blockAt(sender);
        if (!block || m_busy) return;
        if (auto* popup = DuelBlockLengthPopup::create(
                "Block Length", duelBlockName(*block), "Save", DuelBlockLengthPopup::choiceFor(*block),
                [self = Weak<DuelRequestsPopup>(this), id = block->id, name = duelBlockName(*block)](int minutes) {
                    auto owner = self.lock();
                    if (!owner || owner->m_busy) return;
                    owner->busy(true);
                    Service::get().editDuelBlock(
                        id, minutes,
                        owner->answer(fmt::format("{} blocked {}", name, duelBlockLengthText(minutes)), false));
                }))
            popup->show();
    }
    void onUnblock(CCObject* sender) {
        auto const* block = blockAt(sender);
        if (!block || m_busy) return;
        auto const name = duelBlockName(*block);
        createQuickPopup(
            "Unblock?", fmt::format("Allow duel requests from <cy>{}</c> again?", plainAlertText(name)).c_str(),
            "Cancel", "Unblock", [self = Weak<DuelRequestsPopup>(this), id = block->id, name](auto*, bool yes) {
                auto owner = self.lock();
                if (!yes || !owner || owner->m_busy) return;
                owner->busy(true);
                Service::get().removeDuelBlock(id, owner->answer(name + " unblocked", false));
            });
    }
    void onUnblockAll(CCObject*) {
        if (m_busy || m_list.blocks.empty()) return;
        createQuickPopup(
            "Unblock All?",
            fmt::format("Allow duel requests from all <cy>{}</c> blocked players again?", m_list.blocks.size()).c_str(),
            "Cancel", "Unblock All", [self = Weak<DuelRequestsPopup>(this)](auto*, bool yes) {
                auto owner = self.lock();
                if (!yes || !owner || owner->m_busy) return;
                owner->busy(true);
                Service::get().removeDuelBlock({}, owner->answer("Everyone unblocked", false));
            });
    }
    // 토글 콜백은 상태가 바뀌기 전에 호출되므로 현재 값을 반대로 읽는다. Save는 설정을 서버에 저장한다.
    void onEnabledToggle(CCObject*) {
        m_enabled = !m_enabledToggle->isToggled();
        updateSummary();
    }
    void onAnonymousToggle(CCObject*) {
        m_anonymous = !m_anonymousToggle->isToggled();
        updateSummary();
    }
    void onSave(CCObject*) {
        if (!m_loaded || m_busy) return;
        auto const settings = typed();
        say("Saving...", kIce);
        busy(true);
        Service::get().setDuelSettings(
            settings, [self = Weak<DuelRequestsPopup>(this), done = answer("Duel request settings saved", true)](
                          std::optional<DuelBlockList> list, std::string error) mutable {
                bool const ok = list.has_value();
                done(std::move(list), std::move(error));
                auto owner = self.lock();
                if (!owner) return;
                owner->say(ok ? "Saved." : "Not saved.", ok ? ccc3(130, 255, 165) : ccc3(255, 104, 92));
                if (ok && owner->m_saved) owner->m_saved();
            });
    }
    // saved: 설정 저장에 성공했을 때 호출된다.
public:
    static DuelRequestsPopup* create(std::function<void()> saved, bool season2 = false) {
        auto* result = new DuelRequestsPopup;
        if (result->init(std::move(saved), season2)) {
            result->autorelease();
            return result;
        }
        delete result;
        return nullptr;
    }
};
