// Ranked 듀얼(친선 대결) 팝업 두 개: 받은 신청을 보여 주는 RankedDuelRequestPopup,
// 보낸 신청을 기다리는 RankedDuelWaitPopup. 응답/취소/차단은 Service로 서버에 보내고,
// 결과로 받은 RankedState는 onState 콜백으로 위에 넘긴다.
//
// 프로필의 티어/레이팅/글로벌 순위를 한 줄로 만든다. 배치를 마치지 않았으면 "PLACEMENT".
std::string duelTierLine(RankedProfile const& profile) {
    bool const ranked = profile.tier != "Unranked" && profile.placementCompleted >= profile.placementRequired;
    std::string line = ranked ? fmt::format("{}   R{:.0f}", profile.tier, profile.rating) : std::string("PLACEMENT");
    if (profile.globalRank > 0) line += fmt::format("   #{}", profile.globalRank);
    return line;
}

// 받은 듀얼 신청 팝업. Accept / Decline / Ignore(컵 듀얼은 Block) 버튼과 만료 카운트다운을 가진다.
// 신청자가 익명이면 정체를 가리고, 컵 듀얼(m_request.cup)이면 컵과 티어를 보여 준다.
// 응답 없이 닫으면 거절로 처리한다.
class RankedDuelRequestPopup final : public ::versus::DeferredPopup {
    RankedDuelRequest m_request;
    std::function<void(RankedState)> m_onState;
    CCLabelBMFont* m_timer = nullptr;
    CCMenuItemSpriteExtra* m_accept = nullptr;
    CCMenuItemSpriteExtra* m_decline = nullptr;
    CCMenuItemSpriteExtra* m_ignore = nullptr;
    bool m_answered = false;
    bool m_quiet = false;
    bool m_closed = false;
    CCNode* m_fx = nullptr;
    CCPoint const kAt{58.f, 158.f};

    // 양쪽에서 칼 두 자루가 부딪치는 연출. 부딪히는 시점에 spark를 띄운다.
    void clash() {
        for (int side : {-1, 1}) {
            auto* sword = art::glyph(art::Glyph::Sword, ccc3(255, 236, 200));
            sword->setScale(1.25f);
            sword->setPosition({kAt.x + side * 30.f, kAt.y + 26.f});
            sword->setRotation(side * -75.f);
            m_fx->addChild(sword);
            sword->runAction(CCSequence::create(
                CCSpawn::create(CCEaseIn::create(CCMoveTo::create(.26f, ccp(kAt.x + side * 7.f, kAt.y + 24.f)), 2.f),
                                CCEaseIn::create(CCRotateTo::create(.26f, side * -30.f), 2.f), nullptr),
                CCDelayTime::create(.4f),
                CCSpawn::create(CCScaleTo::create(.22f, 0.f), CCMoveBy::create(.22f, {side * 8.f, 5.f}), nullptr),
                CCRemoveSelf::create(), nullptr));
        }
        m_fx->runAction(
            CCSequence::create(CCDelayTime::create(.26f), CallFuncExt::create([this] { spark(); }), nullptr));
    }
    // 칼이 부딪친 자리의 링과 파티클 연출.
    void spark() {
        CCPoint const hit{kAt.x, kAt.y + 26.f};
        auto* ring = CCDrawNode::create();
        art::stroke(ring, art::circle({0.f, 0.f}, 10.f, 28), 1.1f, {1.f, .84f, .4f, 1.f});
        ring->setPosition(hit);
        m_fx->addChild(ring);
        ring->runAction(CCSequence::create(CCEaseSineOut::create(CCScaleTo::create(.35f, 2.4f)),
                                           CCScaleTo::create(.1f, 0.f), CCRemoveSelf::create(), nullptr));
        for (int i = 0; i < 6; ++i) {
            auto* dot = CCDrawNode::create();
            dot->drawDot({0.f, 0.f}, 1.6f, {1.f, .9f, .55f, 1.f});
            dot->setPosition(hit);
            m_fx->addChild(dot);
            float const angle = i * 60.f * art::kPi / 180.f;
            dot->runAction(CCSequence::create(
                CCEaseSineOut::create(CCMoveBy::create(.3f, {std::cos(angle) * 18.f, std::sin(angle) * 18.f})),
                CCScaleTo::create(.12f, 0.f), CCRemoveSelf::create(), nullptr));
        }
    }

    // 응답 중 중복 입력을 막기 위해 세 버튼을 잠그거나 푼다.
    void lockAnswers(bool locked) {
        for (auto* item : {m_accept, m_decline, m_ignore}) {
            item->setEnabled(!locked);
            item->setOpacity(locked ? 120 : 255);
        }
    }
    // 수락/거절을 서버에 보낸다(한 번만). 수락이 실패하면 사유를 알림으로 보이고, 끝나면 팝업을 닫는다.
    void answer(bool accept) {
        if (m_answered) return;
        m_answered = true;
        lockAnswers(true);
        m_timer->setString(accept ? "Starting duel..." : "Declining...");
        Service::get().respondDuel(
            m_request.id, accept,
            [accept, self = Weak<RankedDuelRequestPopup>(this)](std::optional<RankedState> state, std::string error) {
                auto owner = self.lock();
                if (!owner) return;
                if (state && owner->m_onState) owner->m_onState(std::move(*state));
                if (!state && accept && !showMaintenanceNotice(error))
                    FLAlertLayer::create("Duel", (error.empty() ? std::string("Duel unavailable.") : error).c_str(),
                                         "OK")
                        ->show();
                owner->dismiss();
            });
    }
    // 수락은 부정행위 감지로 막혀 있으면 진행하지 않는다.
    void onAccept(CCObject*) {
        if (!m_answered && rankedBlockedByCheat()) return;
        answer(true);
    }
    void onDecline(CCObject*) { answer(false); }
    // 신청자를 일정 시간 무시(차단)한다. 먼저 기간 선택 팝업을 띄우고, 선택하면 서버에 요청한다.
    // 실패하면 버튼을 다시 풀어 준다.
    void onIgnore(CCObject*) {
        if (m_answered) return;
        bool const hidden = m_request.anonymous && !m_request.player;
        std::string const who =
            hidden || !m_request.player ? std::string("Anonymous player") : m_request.player->player.name;
        std::string const verb = m_request.cup.empty() ? "Ignore" : "Block";
        auto chosen = [id = m_request.id, onState = m_onState, who, verb,
                       self = Weak<RankedDuelRequestPopup>(this)](int minutes) {
            if (auto owner = self.lock(); owner && !owner->m_closed) {
                if (owner->m_answered) return;
                owner->m_answered = true;
                owner->lockAnswers(true);
                owner->m_timer->setString("Ignoring...");
            }
            Service::get().ignoreDuel(
                id, minutes, [onState, who, verb, minutes, self](std::optional<RankedState> state, std::string error) {
                    auto owner = self.lock();
                    if (!state) {
                        if (!showMaintenanceNotice(error))
                            FLAlertLayer::create(
                                verb.c_str(),
                                (error.empty() ? std::string("Could not block this player.") : plainAlertText(error))
                                    .c_str(),
                                "OK")
                                ->show();
                        if (owner && !owner->m_closed) {
                            owner->m_answered = false;
                            owner->lockAnswers(false);
                        }
                        return;
                    }
                    if (onState) onState(std::move(*state));
                    Notification::create(
                        fmt::format("Duel requests from {} blocked {}", who, duelBlockLengthText(minutes)),
                        NotificationIcon::Success, 2.5f)
                        ->show();
                    if (owner) owner->dismiss();
                });
        };
        auto* picker = m_request.cup.empty()
                           ? DuelBlockLengthPopup::create("Ignore", who, "Ignore", kDuelBlockDefault, std::move(chosen))
                           : DuelBlockLengthPopup::create("Block", who, "Block", kDuelBlockDefault, std::move(chosen));
        if (picker) picker->show();
    }
    // 버튼으로 응답하지 않고 닫으면 거절로 처리한다.
    void onClose(CCObject* sender) override {
        if (!m_quiet && !m_answered) {
            answer(false);
            return;
        }
        Popup::onClose(sender);
    }
    // 서버 시간 기준 남은 만료 시간을 초 단위(올림)로 표시한다.
    void update(float) override {
        if (m_answered) return;
        auto left = std::max<int64_t>(0, m_request.expiresAt - Service::get().serverNow());
        m_timer->setString(fmt::format("Expires in {}s", (left + 999) / 1000).c_str());
    }
    // 신청자 정보(익명 여부, 컵 듀얼 여부에 따라 다름), 버튼, 연출 노드를 배치하고 카운트다운을 시작한다.
    bool init(RankedDuelRequest request, std::function<void(RankedState)> onState) {
        if (!Popup::init(300.f, 236.f, "GJ_square02.png")) return false;
        m_request = std::move(request);
        m_onState = std::move(onState);
        setTitle("DUEL REQUEST", "goldFont.fnt", .75f);
        m_title->setZOrder(2);
        auto const profile = m_request.player.value_or(RankedProfile{});
        bool const hidden = m_request.anonymous && !m_request.player;
        rankedPanel(m_mainLayer, {10.f, 72.f}, {280.f, 128.f}, ccc3(15, 22, 48), 170)->setZOrder(0);
        if (hidden) {
            PlayerProfile shadow;
            shadow.name = "Anonymous";
            shadow.color1 = 0x3C3C4A;
            shadow.color2 = 0x707084;
            player(m_mainLayer, shadow, {58.f, 158.f}, 1.15f);
            label(m_mainLayer, "?", {58.f, 158.f}, .7f, 30.f, ccWHITE, false, "bigFont.fnt")->setZOrder(5);
        } else
            player(m_mainLayer, profile.player, {58.f, 158.f}, 1.15f);
        m_fx = CCNode::create();
        m_mainLayer->addChild(m_fx, 4);
        m_fx->runAction(CCRepeatForever::create(
            CCSequence::create(CallFuncExt::create([this] { clash(); }), CCDelayTime::create(1.3f), nullptr)));
        if (hidden) {
            label(m_mainLayer, "ANONYMOUS", {178.f, 172.f}, .5f, 200.f, ccc3(200, 200, 215), false, "bigFont.fnt");
            label(m_mainLayer, "Identity hidden until you accept", {178.f, 146.f}, .3f, 200.f, ccWHITE, false,
                  "bigFont.fnt");
            label(m_mainLayer, "The challenger chose to stay anonymous", {178.f, 122.f}, .32f, 200.f, kIce, false,
                  "chatFont.fnt");
        } else {
            rankedNickname(m_mainLayer, profile.player, profile.globalRank, {178.f, 178.f}, .48f, 200.f);
            titleBadge(m_mainLayer, profile.player.cosmetics.title, {178.f, 160.f}, .95f, 200.f);
            if (!m_request.cup.empty()) {
                season2Face(m_mainLayer, m_request.tier, {58.f, 104.f}, .5f);
                label(m_mainLayer, "DUEL AT", {178.f, 141.f}, .3f, 200.f, kIce, false, "bigFont.fnt");
                auto* tier = Season2TierText::create();
                tier->setPosition({178.f, 125.f});
                m_mainLayer->addChild(tier, 3);
                tier->set(m_request.tierName, m_request.tier, .4f, 200.f, false);
                label(m_mainLayer, m_request.cupName, {178.f, 108.f}, .42f, 200.f, kIce, false, "chatFont.fnt");
            } else {
                auto* tier = label(m_mainLayer, duelTierLine(profile), {178.f, 139.f}, .34f, 200.f, ccWHITE, false,
                                   "bigFont.fnt");
                rankedGradient(tier, profile.tier);
                rankedFace(m_mainLayer, profile.tier, {58.f, 104.f}, .5f);
                label(m_mainLayer,
                      fmt::format("{}W  {}L   |   WIN RATE {}", profile.wins, profile.losses, rate(profile.player)),
                      {178.f, 118.f}, .32f, 200.f, kIce, false, "chatFont.fnt");
            }
        }
        label(m_mainLayer,
              m_request.cup.empty() ? "wants to duel you - rating range ignored"
                                    : "wants to duel you - ratings never change",
              {178.f, 95.f}, .3f, 200.f, ccc3(255, 214, 120), false, "chatFont.fnt");
        m_timer = label(m_mainLayer, "", {150.f, 60.f}, .3f, 200.f, kMuted, false, "chatFont.fnt");
        auto* actions = menu(m_mainLayer);
        m_ignore = m_request.cup.empty() ? button(actions, this, menu_selector(RankedDuelRequestPopup::onIgnore),
                                                  "Ignore", {54.f, 30.f}, .62f, "GJ_button_04.png")
                                         : button(actions, this, menu_selector(RankedDuelRequestPopup::onIgnore),
                                                  "Block", {54.f, 30.f}, .62f, "GJ_button_04.png");
        m_ignore->setID("duel-request-ignore"_spr);
        m_decline = button(actions, this, menu_selector(RankedDuelRequestPopup::onDecline), "Decline", {150.f, 30.f},
                           .62f, "GJ_button_06.png");
        m_accept = button(actions, this, menu_selector(RankedDuelRequestPopup::onAccept), "Accept", {246.f, 30.f}, .62f,
                          "GJ_button_01.png");
        update(0.f);
        scheduleUpdate();
        return true;
    }

public:
    std::string const& requestId() const { return m_request.id; }
    // 응답 처리 없이 조용히 닫는다(서버가 이미 취소/만료한 경우 등에 쓰인다).
    void dismiss() {
        if (m_closed) return;
        m_closed = m_quiet = true;
        if (m_fx) m_fx->stopAllActions();
        Popup::onClose(nullptr);
    }
    static RankedDuelRequestPopup* create(RankedDuelRequest request, std::function<void(RankedState)> onState) {
        auto* result = new RankedDuelRequestPopup;
        if (result->init(std::move(request), std::move(onState))) {
            result->autorelease();
            return result;
        }
        delete result;
        return nullptr;
    }
};

// 내가 보낸 듀얼 신청을 상대가 받을 때까지 기다리는 팝업. 칼이 날아가는 연출을 반복하고
// 남은 시간을 보여 준다. Cancel 버튼이나 닫기는 신청 취소로 처리한다.
class RankedDuelWaitPopup final : public ::versus::DeferredPopup {
    RankedDuelRequest m_request;
    std::function<void(RankedState)> m_onState;
    CCLabelBMFont* m_status = nullptr;
    CCNode* m_flight = nullptr;
    float m_time = 0.f;
    bool m_canceling = false;
    bool m_quiet = false;
    CCPoint const kFrom{62.f, 128.f}, kTo{238.f, 128.f};
    bool m_closed = false;

    // 칼이 곡선을 그리며 상대 쪽으로 날아가는 연출과 꼬리 불꽃.
    void launch() {
        auto* sword = art::glyph(art::Glyph::Sword, ccc3(255, 236, 200));
        sword->setScale(1.5f);
        sword->setPosition(kFrom);
        sword->setRotation(-30.f);
        m_flight->addChild(sword);
        ccBezierConfig arc;
        arc.controlPoint_1 = ccp(kFrom.x + 45.f, kFrom.y + 46.f);
        arc.controlPoint_2 = ccp(kTo.x - 45.f, kTo.y + 46.f);
        arc.endPosition = kTo;
        sword->runAction(
            CCSequence::create(CCSpawn::create(CCEaseSineInOut::create(CCBezierTo::create(1.05f, arc)),
                                               CCRotateBy::create(1.05f, 60.f), nullptr),
                               CallFuncExt::create([this] { pulse(); }),
                               CCSpawn::create(CCScaleTo::create(.18f, 2.2f), CCFadeOut::create(.18f), nullptr),
                               CCRemoveSelf::create(), nullptr));
        for (int i = 1; i <= 4; ++i) {
            auto* spark = CCDrawNode::create();
            spark->drawDot({0.f, 0.f}, 2.2f - i * .35f, {1.f, .82f, .45f, .9f - i * .15f});
            spark->setPosition(kFrom);
            m_flight->addChild(spark);
            spark->runAction(CCSequence::create(CCDelayTime::create(i * .06f),
                                                CCEaseSineInOut::create(CCBezierTo::create(1.05f, arc)),
                                                CCRemoveSelf::create(), nullptr));
        }
    }
    // 칼이 도착한 자리에 링을 띄운다.
    void pulse() {
        auto* ring = CCDrawNode::create();
        art::stroke(ring, art::circle({0.f, 0.f}, 14.f, 32), 1.2f, {1.f, .84f, .4f, 1.f});
        ring->setPosition(kTo);
        m_flight->addChild(ring);
        ring->runAction(CCSequence::create(CCEaseSineOut::create(CCScaleTo::create(.45f, 2.f)),
                                           CCScaleTo::create(.12f, 0.f), CCRemoveSelf::create(), nullptr));
    }
    // 신청 취소를 서버에 보내고 응답이 오면 상태를 전달한 뒤 닫는다.
    void onCancel(CCObject*) {
        if (m_canceling) return;
        m_canceling = true;
        m_status->setString("Canceling...");
        Service::get().cancelDuel(
            m_request.id, [self = Weak<RankedDuelWaitPopup>(this)](std::optional<RankedState> state, std::string) {
                auto owner = self.lock();
                if (!owner) return;
                if (state && owner->m_onState) owner->m_onState(std::move(*state));
                owner->dismiss();
            });
    }
    // 닫기 버튼도 취소로 처리한다.
    void onClose(CCObject* sender) override {
        if (!m_quiet) {
            onCancel(sender);
            return;
        }
        Popup::onClose(sender);
    }
    // 점 애니메이션과 남은 시간을 상태 라벨에 갱신한다.
    void update(float dt) override {
        m_time += dt;
        if (m_canceling) return;
        static constexpr char const* dots[] = {"", ".", "..", "..."};
        auto left = std::max<int64_t>(0, m_request.expiresAt - Service::get().serverNow());
        std::string name = m_request.player ? m_request.player->player.name : std::string("opponent");
        m_status->setString(fmt::format("Waiting for {}{}   ({}s)", name, dots[static_cast<int>(m_time * 2.5f) % 4],
                                        (left + 999) / 1000)
                                .c_str());
        m_status->limitLabelWidth(260.f, .34f, .2f);
    }
    // 보낸 사람과 받는 사람을 마주 보게 배치하고, 컵 듀얼이면 컵/티어를, 아니면 상대 티어 줄을 보인다.
    bool init(RankedDuelRequest request, std::function<void(RankedState)> onState) {
        if (!Popup::init(300.f, 210.f, "GJ_square02.png")) return false;
        m_request = std::move(request);
        m_onState = std::move(onState);
        setTitle("DUEL REQUEST SENT", "goldFont.fnt", .7f);
        m_title->setZOrder(2);
        rankedPanel(m_mainLayer, {10.f, 66.f}, {280.f, 110.f}, ccc3(15, 22, 48), 170)->setZOrder(0);
        auto const self = Service::get().profile();
        auto const other = m_request.player.value_or(RankedProfile{});
        player(m_mainLayer, self, kFrom, 1.f)->setZOrder(3);
        auto* target = player(m_mainLayer, other.player, kTo, 1.f);
        target->setZOrder(3);
        target->setFlipX(true);
        label(m_mainLayer, self.name, {kFrom.x, 97.f}, .34f, 110.f, ccWHITE, false, "bigFont.fnt");
        rankedNickname(m_mainLayer, other.player, other.globalRank, {kTo.x, 97.f}, .34f, 110.f);
        if (!m_request.cup.empty()) {
            auto* tier = Season2TierText::create();
            tier->setPosition({150.f, 82.f});
            m_mainLayer->addChild(tier, 3);
            tier->set(m_request.tierName, m_request.tier, .3f, 120.f, false);
            label(m_mainLayer, fmt::format("{}  |  no rating change", m_request.cupName), {150.f, 68.f}, .34f, 200.f,
                  kIce, false, "chatFont.fnt");
        } else {
            auto* tier =
                label(m_mainLayer, duelTierLine(other), {kTo.x, 80.f}, .24f, 110.f, ccWHITE, false, "bigFont.fnt");
            rankedGradient(tier, other.tier);
        }
        m_flight = CCNode::create();
        m_mainLayer->addChild(m_flight, 4);
        m_flight->runAction(CCRepeatForever::create(
            CCSequence::create(CallFuncExt::create([this] { launch(); }), CCDelayTime::create(1.4f), nullptr)));
        m_status = label(m_mainLayer, "", {150.f, 52.f}, .34f, 260.f, kIce, false, "chatFont.fnt");
        if (m_request.anonymous)
            label(m_mainLayer, "SENT ANONYMOUSLY", {150.f, 72.f}, .22f, 200.f, ccc3(200, 200, 215), false,
                  "bigFont.fnt");
        button(menu(m_mainLayer), this, menu_selector(RankedDuelWaitPopup::onCancel), "Cancel", {150.f, 24.f}, .7f,
               "GJ_button_06.png");
        update(0.f);
        scheduleUpdate();
        return true;
    }

public:
    std::string const& requestId() const { return m_request.id; }
    // 서버 호출 없이 조용히 닫는다.
    void dismiss() {
        if (m_closed) return;
        m_closed = m_quiet = true;
        m_flight->stopAllActions();
        Popup::onClose(nullptr);
    }
    static RankedDuelWaitPopup* create(RankedDuelRequest request, std::function<void(RankedState)> onState) {
        auto* result = new RankedDuelWaitPopup;
        if (result->init(std::move(request), std::move(onState))) {
            result->autorelease();
            return result;
        }
        delete result;
        return nullptr;
    }
};
