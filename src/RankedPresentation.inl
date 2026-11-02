// Ranked 연출 UI 세 가지: 매치 발견 연출 RankedClash, 듀얼 신청 확인 팝업 DuelConfirmPopup,
// 매칭 대기 중 탐색 중인 플레이어를 레이팅 축 위에 보여 주는 RankedSearchBoard.
//
// "MATCH FOUND" 전체 화면 연출. 내 카드와 상대 카드가 양쪽에서 날아와 가운데에서 부딪히고
// VS 표시와 충격 이펙트가 나온 뒤(MATCH_INTRO_SECONDS 길이) 서서히 사라지며 스스로 제거된다.
class RankedClash final : public CCLayer {
    RankedProfile m_self, m_opponent;
    // 노드 트리 전체를 duration 안에 사라지게 한다. 입자는 남은 수명을 줄여 함께 꺼지게 한다.
    static void fadeTree(CCNode* node, float duration) {
        node->stopAllActions();
        if (auto* particles = typeinfo_cast<CCParticleSystem*>(node)) {
            particles->stopSystem();
            for (unsigned i = 0; i < particles->getParticleCount(); ++i) {
                auto& p = particles->m_pParticles[i];
                p.timeToLive = std::min(p.timeToLive, duration);
                p.deltaColor.a = -p.color.a / std::max(.001f, p.timeToLive);
            }
        } else if (typeinfo_cast<CCNodeRGBA*>(node) || typeinfo_cast<CCLayerRGBA*>(node)) {
            node->runAction(CCFadeOut::create(duration));
        }
        for (auto* child : CCArrayExt<CCNode*>(node->getChildren())) fadeTree(child, duration);
    }
    void onSelf(CCObject*) {
        if (auto* popup = PlayerPopup::create(m_self.player)) popup->show();
    }
    void onOpponent(CCObject*) {
        if (auto* popup = PlayerPopup::create(m_opponent.player)) popup->show();
    }
    // 플레이어 카드 한 장(아바타, 닉네임, 칭호, 티어 얼굴과 레이팅 또는 배치 진행도, 정보 버튼).
    // 가운데 쪽 가장자리는 톱니 모양으로 잘라 두 카드가 맞물리게 한다. right가 상대 쪽 카드.
    CCNode* card(RankedProfile const& profile, bool right, CCSize size) {
        auto* root = CCNode::create();
        root->setContentSize(size);
        auto* stencil = CCDrawNode::create();
        for (int i = 0; i < 10; ++i) {
            float y0 = i * size.height / 10.f, y1 = (i + 1) * size.height / 10.f;
            float a = (i % 2 ? -5.f : 5.f), b = (i % 2 ? 5.f : -5.f);
            CCPoint points[4];
            if (right) {
                points[0] = CCPoint{a, y0};
                points[1] = CCPoint{size.width, y0};
                points[2] = CCPoint{size.width, y1};
                points[3] = CCPoint{b, y1};
            } else {
                points[0] = CCPoint{0, y0};
                points[1] = CCPoint{size.width + a, y0};
                points[2] = CCPoint{size.width + b, y1};
                points[3] = CCPoint{0, y1};
            }
            stencil->drawPolygon(points, 4, {1, 1, 1, 1}, 0, {1, 1, 1, 1});
        }
        auto* clip = CCClippingNode::create(stencil);
        root->addChild(clip);
        auto* bg =
            CCLayerGradient::create(right ? ccc4(4, 110, 168, 255) : ccc4(224, 99, 16, 255),
                                    right ? ccc4(2, 31, 64, 255) : ccc4(71, 26, 9, 255), {right ? 1.f : -1.f, -1.f});
        bg->setContentSize({size.width + 14.f, size.height});
        bg->setPositionX(-7.f);
        clip->addChild(bg);
        float x = size.width / 2.f;
        auto* avatar = player(root, profile.player, {x, size.height * .55f}, 2.f);
        if (right) avatar->setScaleX(-2.f);
        rankedNickname(root, profile.player, profile.globalRank, {x, size.height * .31f}, .6f, size.width - 30.f);
        titleBadge(root, profile.player.cosmetics.title, {x, size.height * .235f}, 1.1f, size.width - 40.f);
        rankedFace(root, profile.tier, {x - 45.f, size.height * .16f}, .65f);
        rankedGradient(label(root,
                             profile.placed
                                 ? fmt::format("R{:.0f}", profile.rating)
                                 : fmt::format("PLACEMENT\n{} / {}", profile.placementCompleted,
                                               profile.placementRequired > 0 ? profile.placementRequired : 5),
                             {x + 20.f, size.height * .16f}, .6f, 85.f, ccWHITE, false, "bigFont.fnt"),
                       profile.tier);
        auto* actions = menu(root);
        iconButton(actions, this, right ? menu_selector(RankedClash::onOpponent) : menu_selector(RankedClash::onSelf),
                   "GJ_infoIcon_001.png", {x, size.height - 29.f}, 26.f);
        return root;
    }

public:
    // 연출 타임라인: 문구 확대 후 사라짐 -> 0.75초 뒤 카드가 슬라이드 -> 0.5초 뒤 충돌
    // (화면 흔들림, 이음선, VS, 번쩍임, 충격 링) -> 전체 페이드 아웃 후 제거. 듀얼이면 안내 배너를 함께 띄운다.
    bool init(RankedProfile self, RankedProfile opponent, bool duel) {
        if (!CCLayer::init()) return false;
        m_self = std::move(self);
        m_opponent = std::move(opponent);
        auto size = CCDirector::sharedDirector()->getWinSize();
        auto* shade = CCLayerColor::create(ccc4(0, 0, 0, 0));
        addChild(shade);
        shade->runAction(CCFadeTo::create(.3f, 235));
        auto* found = label(this, duel ? "DUEL ACCEPTED!" : "MATCH FOUND!", size / 2.f, 1.f, size.width - 60.f, ccWHITE,
                            false, "gjFont46.fnt");
        if (duel) {
            auto* banner =
                label(this, "DUEL MATCH  -  paired by request, regardless of rating", {size.width / 2.f, 12.f}, .42f,
                      size.width - 40.f, ccc3(255, 214, 120), false, "bigFont.fnt");
            banner->setZOrder(8);
            banner->setOpacity(0);
            banner->runAction(CCSequence::create(CCDelayTime::create(.3f), CCFadeIn::create(.25f), nullptr));
        }
        rankedTitleGradient(found);
        found->setScale(.1f);
        found->runAction(CCSequence::create(CCEaseBackOut::create(CCScaleTo::create(.3f, .95f)),
                                            CCDelayTime::create(.35f), CCFadeOut::create(.2f), nullptr));
        audio::play(audio::Cue::MatchFound);
        float w = size.width / 2.f, h = size.height - 50.f;
        auto* left = card(m_self, false, {w, h});
        auto* right = card(m_opponent, true, {w, h});
        left->setPosition({-w - 20.f, 25.f});
        right->setPosition({size.width + 20.f, 25.f});
        addChild(left, 2);
        addChild(right, 2);
        left->runAction(CCSequence::create(CCDelayTime::create(.75f),
                                           CCEaseSineInOut::create(CCMoveTo::create(.5f, {0.f, 25.f})), nullptr));
        right->runAction(CCSequence::create(CCDelayTime::create(.75f),
                                            CCEaseSineInOut::create(CCMoveTo::create(.5f, {w, 25.f})), nullptr));
        runAction(CCSequence::create(
            CCDelayTime::create(.75f), CallFuncExt::create([] { audio::play(audio::Cue::SwordFly); }),
            CCDelayTime::create(.5f), CallFuncExt::create([this, size, left, right] {
                audio::play(audio::Cue::SwordImpact);
                this->runAction(CCSequence::create(
                    CCMoveBy::create(.035f, {7.f, 4.f}), CCMoveBy::create(.05f, {-13.f, -7.f}),
                    CCMoveBy::create(.05f, {9.f, 5.f}), CCMoveBy::create(.05f, {-3.f, -2.f}), nullptr));
                auto* seam = CCNode::create();
                for (int i = 0; i < 10; ++i) {
                    float y = 25.f + i * (size.height - 50.f) / 10.f;
                    CCPoint a{size.width / 2.f + (i % 2 ? -5.f : 5.f), y};
                    CCPoint b{size.width / 2.f + (i % 2 ? 5.f : -5.f), y + (size.height - 50.f) / 10.f};
                    auto delta = b - a;
                    auto* segment = CCLayerColor::create(ccc4(255, 224, 153, 255), delta.getLength() + 1.f, 4.f);
                    segment->ignoreAnchorPointForPosition(false);
                    segment->setAnchorPoint({.5f, .5f});
                    segment->setPosition((a + b) / 2.f);
                    segment->setRotation(-std::atan2(delta.y, delta.x) * 180.f / 3.14159265f);
                    seam->addChild(segment);
                }
                addChild(seam, 4);
                auto* vs = label(this, "VS", size / 2.f, .95f, 80.f, ccWHITE, false, "gjFont46.fnt");
                vs->setZOrder(7);
                vs->setScale(2.5f);
                vs->runAction(CCEaseBackOut::create(CCScaleTo::create(.3f, .95f)));
                auto* flash = CCLayerColor::create(ccc4(255, 244, 219, 190));
                addChild(flash, 10);
                flash->runAction(CCSequence::create(CCFadeOut::create(.25f), CCRemoveSelf::create(), nullptr));
                rankedImpact(this, size / 2.f, 95.f);
            }),
            CCDelayTime::create(ranked::MATCH_INTRO_SECONDS - 1.8f), CallFuncExt::create([this] {
                for (auto* child : CCArrayExt<CCNode*>(getChildren())) fadeTree(child, .55f);
            }),
            CCDelayTime::create(.55f), CCRemoveSelf::create(), nullptr));
        return true;
    }
    static RankedClash* create(RankedProfile self, RankedProfile opponent, bool duel = false) {
        auto* result = new RankedClash;
        if (result->init(std::move(self), std::move(opponent), duel)) {
            result->autorelease();
            return result;
        }
        delete result;
        return nullptr;
    }
};

// 듀얼 신청 전 확인 팝업. 익명으로 보내기 선택은 모드 저장값(duel-anonymous)에 기억한다.
// 상대가 익명 신청을 받지 않으면 선택할 수 없다. Send를 누르면 닫고 선택값을 콜백에 넘긴다.
class DuelConfirmPopup final : public ::versus::DeferredPopup {
    std::function<void(bool)> m_send;
    CCMenuItemToggler* m_anonymous = nullptr;
    bool m_hidden = false;
    bool m_anonymousAllowed = true;
    // 익명 토글 처리. 익명을 받지 않는 상대면 값을 바꾸지 않는다.
    void onAnonymous(CCObject*) {
        if (!m_anonymousAllowed) {
            m_anonymous->toggle(true);
            return;
        }
        m_hidden = !m_anonymous->isToggled();
    }
    void onSend(CCObject*) {
        auto send = std::move(m_send);
        bool const hidden = m_hidden;
        Popup::onClose(nullptr);
        if (send) send(hidden);
    }
    void onCancel(CCObject*) { Popup::onClose(nullptr); }
    bool init(PlayerProfile const& target, bool anonymousAllowed, std::function<void(bool)> send) {
        if (!Popup::init(320.f, 214.f, "GJ_square02.png")) return false;
        m_send = std::move(send);
        m_anonymousAllowed = anonymousAllowed;
        m_hidden = anonymousAllowed && Mod::get()->getSavedValue<bool>("duel-anonymous", false);
        setTitle("DUEL", "goldFont.fnt", .75f);
        player(m_mainLayer, target, {50.f, 150.f}, .8f);
        rankedNickname(m_mainLayer, target, 0, {50.f, 124.f}, .3f, 80.f);
        label(m_mainLayer, "Send a duel request to", {200.f, 166.f}, .3f, 200.f, ccWHITE, false, "bigFont.fnt");
        label(m_mainLayer, target.name + "?", {200.f, 149.f}, .42f, 200.f, ccc3(255, 230, 120), false, "bigFont.fnt");
        label(m_mainLayer, "If they accept, the match starts right away, at any rating.", {200.f, 130.f}, .5f, 205.f,
              kIce, false, "chatFont.fnt");
        label(m_mainLayer, "After a duel, play 2 other Ranked matches", {200.f, 116.f}, .5f, 205.f, ccc3(255, 175, 90),
              false, "chatFont.fnt");
        label(m_mainLayer, "before dueling the same player again.", {200.f, 104.f}, .5f, 205.f, ccc3(255, 175, 90),
              false, "chatFont.fnt");
        auto* actions = menu(m_mainLayer);
        m_anonymous =
            CCMenuItemToggler::createWithStandardSprites(this, menu_selector(DuelConfirmPopup::onAnonymous), .55f);
        m_anonymous->toggle(m_hidden);
        m_anonymous->setPosition({60.f, 76.f});
        actions->addChild(m_anonymous);
        label(m_mainLayer, "Send anonymously", {76.f, 80.f}, .34f, 160.f, ccWHITE, true, "bigFont.fnt");
        if (m_anonymousAllowed)
            label(m_mainLayer, "Your name stays hidden until they accept", {76.f, 66.f}, .3f, 230.f, kMuted, true,
                  "chatFont.fnt");
        else {
            m_anonymous->setEnabled(false);
            previewOpacity(m_anonymous, 110);
            label(m_mainLayer, "This player does not accept anonymous requests", {76.f, 66.f}, .3f, 230.f,
                  ccc3(255, 175, 90), true, "chatFont.fnt");
        }
        button(actions, this, menu_selector(DuelConfirmPopup::onCancel), "Cancel", {95.f, 28.f}, .62f,
               "GJ_button_06.png");
        button(actions, this, menu_selector(DuelConfirmPopup::onSend), "Send", {225.f, 28.f}, .62f, "GJ_button_01.png");
        return true;
    }

public:
    static DuelConfirmPopup* create(PlayerProfile const& target, bool anonymousAllowed,
                                    std::function<void(bool)> send) {
        auto* result = new DuelConfirmPopup;
        if (result->init(target, anonymousAllowed, std::move(send))) {
            result->autorelease();
            return result;
        }
        delete result;
        return nullptr;
    }
};

// 매칭 대기 화면의 라이브 보드. 레이팅 축 위에 탐색 중인 플레이어 아바타를 놓고,
// 내 탐색 범위(레이팅 구간)를 띠로 보여 준다. 다른 플레이어는 눌러서 프로필을 열거나 듀얼을 신청할 수 있고,
// 이모트 말풍선을 띄운다. Season 2 룩에서는 축 대신 티어 컷오프 눈금을 쓴다.
class RankedSearchBoard final : public CCNode {
    CCSize m_size;
    CCLayerColor* m_range = nullptr;
    CCNode* m_players = nullptr;
    CCNode* m_axis = nullptr;
    std::array<CCLabelBMFont*, 6> m_ticks{};
    std::string m_signature;
    std::string m_boardKey;
    uint64_t m_boardRevision = 0;
    AvatarCache m_avatars;
    float m_halfRange = 100.f;
    float m_rosterTimer = 0.f;
    CCMenu* m_hits = nullptr;
    std::vector<PlayerProfile> m_hitProfiles;
    std::vector<char> m_hitAnonymous;
    CCMenu* m_options = nullptr;
    CCLabelBMFont* m_duelLabel = nullptr;
    double m_axisMax = ranked::BOARD_RATING_MAX;
    CCMenuItemToggler* m_duelToggle = nullptr;
    CCLabelBMFont* m_caption = nullptr;
    bool m_duelPending = false;
    std::function<void(RankedState)> m_onState;
    // uid별 아이콘 위치(이모트 말풍선 위치)와 이미 표시한 이모트 시각, 내가 직접 보낸 이모트 시각.
    std::unordered_map<std::string, CCPoint> m_iconAt;
    std::unordered_map<std::string, int64_t> m_emoteSeen;
    int64_t m_localEmoteAt = 0;
    CCNode* m_bubbles = nullptr;
    // 해당 플레이어 아이콘 위에 이모트 말풍선을 잠깐 띄운다.
    void popEmote(std::string const& uid, std::string const& kind) {
        auto found = m_iconAt.find(uid);
        if (found == m_iconAt.end() || !m_bubbles || !emotes::find(kind)) return;
        auto const at = found->second + CCPoint{0.f, 27.f};
        auto* bubble = CCNodeRGBA::create();
        bubble->setCascadeOpacityEnabled(true);
        bubble->setPosition(at);
        bubble->setScale(.1f);
        m_bubbles->addChild(bubble);
        auto* bg = NineSlice::create("square02b_001.png");
        bg->setContentSize({30.f, 28.f});
        bg->setColor(ccWHITE);
        bubble->addChild(bg);
        auto* art = emoteArt(kind);
        art->setScale(.58f);
        bubble->addChild(art, 2);
        emoteBurst(m_bubbles, at, kind, .62f);
        bubble->runAction(
            CCSequence::create(CCEaseBackOut::create(CCScaleTo::create(.2f, 1.f)), CCDelayTime::create(1.5f),
                               CCSpawn::create(CCFadeOut::create(.25f), CCScaleTo::create(.25f, .7f), nullptr),
                               CCRemoveSelf::create(), nullptr));
        art->runAction(
            CCRepeat::create(CCSequence::create(CCEaseSineInOut::create(CCRotateTo::create(.12f, -9.f)),
                                                CCEaseSineInOut::create(CCRotateTo::create(.12f, 9.f)),
                                                CCEaseSineInOut::create(CCRotateTo::create(.12f, 0.f)), nullptr),
                             2));
    }
    // 서버 상태의 탐색자 이모트를 보여 준다. 이미 본 것, 4초 넘게 지난 것, 내가 방금 로컬로 띄운 것은 건너뛴다.
    void showEmotes(RankedState const& state) {
        auto const now = Service::get().serverNow();
        auto const& own = Service::get().profile().uid;
        for (auto const& entry : state.searchers) {
            if (entry.emoteKind.empty() || entry.emoteAt <= 0) continue;
            auto& seen = m_emoteSeen[entry.player.uid];
            if (seen == entry.emoteAt) continue;
            seen = entry.emoteAt;
            if (now - entry.emoteAt > 4'000) continue;
            if (entry.player.uid == own && std::llabs(entry.emoteAt - m_localEmoteAt) < 5'000) continue;
            popEmote(entry.player.uid, entry.emoteKind);
        }
    }
    // 아바타 위에 눌러서 프로필을 여는 투명 터치 영역을 놓는다.
    void addHit(PlayerProfile const& profile, CCPoint at) {
        auto* area = CCNode::create();
        area->setContentSize({20.f, 30.f});
        auto* item = CCMenuItemSpriteExtra::create(area, this, menu_selector(RankedSearchBoard::onPick));
        item->setTag(static_cast<int>(m_hitProfiles.size()));
        item->setPosition(at);
        m_hitProfiles.push_back(profile);
        m_hitAnonymous.push_back(true);
        m_hits->addChild(item);
    }
    // 아바타 옆에 듀얼 신청 버튼(칼 아이콘)을 놓는다. anonymousAllowed는 상대가 익명 신청을 받는지.
    void addDuel(PlayerProfile const& profile, CCPoint at, bool anonymousAllowed = true) {
        auto* sprite = CCNode::create();
        sprite->setContentSize({16.f, 16.f});
        auto* disc = CCDrawNode::create();
        art::disc(disc, {8.f, 8.f}, 7.f, art::f4(ccc3(170, 34, 64)), .9f);
        disc->drawDot({8.f, 9.5f}, 5.f, art::f4(ccc3(235, 90, 120), .55f));
        art::stroke(disc, art::circle({8.f, 8.f}, 7.f, 24), .35f, {1.f, .85f, .9f, .8f});
        sprite->addChild(disc);
        auto* sword = art::glyph(art::Glyph::Sword, ccc3(255, 235, 240));
        sword->setScale(1.1f);
        sword->setPosition({8.f, 8.f});
        sprite->addChild(sword);
        sprite->setScale(.8f);
        sprite->setPosition({11.f - 6.4f, 9.f - 6.4f});
        auto* target = CCNode::create();
        target->setContentSize({22.f, 18.f});
        target->addChild(sprite);
        auto* item = CCMenuItemSpriteExtra::create(target, this, menu_selector(RankedSearchBoard::onDuel));
        item->setTag(static_cast<int>(m_hitProfiles.size()));
        item->setPosition(at);
        item->setID(fmt::format("duel-{}", m_hitProfiles.size()));
        m_hitProfiles.push_back(profile);
        m_hitAnonymous.push_back(anonymousAllowed);
        m_hits->addChild(item, -1);
    }
    // 듀얼 신청. 부정행위 감지로 막혀 있으면 무시하고, 확인 팝업 후 서버에 신청을 보낸다.
    // 실패하면 사유를 알림으로 보이고, 성공하면 갱신된 상태를 위로 전달한다.
    void onDuel(CCObject* sender) {
        if (rankedBlockedByCheat()) return;
        int const index = sender->getTag();
        if (index < 0 || index >= static_cast<int>(m_hitProfiles.size())) return;
        auto target = m_hitProfiles[index];
        bool const anonymousAllowed =
            index < static_cast<int>(m_hitAnonymous.size()) ? m_hitAnonymous[index] != 0 : true;
        if (auto* popup = DuelConfirmPopup::create(
                target, anonymousAllowed,
                [target, anonymousAllowed, self = Weak<RankedSearchBoard>(this)](bool anonymous) {
                    if (rankedBlockedByCheat()) return;
                    if (anonymousAllowed) Mod::get()->setSavedValue("duel-anonymous", anonymous);
                    Service::get().challengeRanked(
                        target.uid, anonymous, [self](std::optional<RankedState> state, std::string error) {
                            if (!state) {
                                if (!showMaintenanceNotice(error))
                                    FLAlertLayer::create(
                                        "Duel", (error.empty() ? std::string("Duel unavailable.") : error).c_str(),
                                        "OK")
                                        ->show();
                                return;
                            }
                            if (auto board = self.lock(); board && board->m_onState)
                                board->m_onState(std::move(*state));
                        });
                }))
            popup->show();
    }
    // 듀얼 신청을 받을지 여부를 서버에 설정한다. 실패하면 토글을 되돌린다.
    void onDuelToggle(CCObject*) {
        if (!m_duelToggle) return;
        bool const enabled = !m_duelToggle->isToggled();
        m_duelPending = true;
        Service::get().setRankedDuels(enabled, [enabled, self = Weak<RankedSearchBoard>(this)](
                                                   std::optional<RankedState> state, std::string error) {
            auto board = self.lock();
            if (!board) return;
            board->m_duelPending = false;
            if (!state) {
                if (board->m_duelToggle) board->m_duelToggle->toggle(!enabled);
                if (!showMaintenanceNotice(error))
                    FLAlertLayer::create("Duels", (error.empty() ? std::string("Setting unavailable.") : error).c_str(),
                                         "OK")
                        ->show();
                return;
            }
            if (board->m_onState) board->m_onState(std::move(*state));
        });
    }
    // 듀얼 신청 관리 팝업을 열고, 닫힐 때 Ranked 상태를 다시 받아 온다.
    void onManageDuels(CCObject*) {
        if (auto* popup = DuelRequestsPopup::create([self = Weak<RankedSearchBoard>(this)] {
                Service::get().fetchRankedState([self](std::optional<RankedState> state, std::string) {
                    if (auto board = self.lock(); board && state && board->m_onState)
                        board->m_onState(std::move(*state));
                });
            }))
            popup->show();
    }
    // 아바타를 누르면 그 플레이어의 프로필 팝업을 연다.
    void onPick(CCObject* sender) {
        int const index = sender->getTag();
        if (index >= 0 && index < static_cast<int>(m_hitProfiles.size()))
            if (auto* popup = PlayerPopup::create(m_hitProfiles[index])) popup->show();
    }
    // 축의 눈금에서 아이콘까지 이어지는 가는 선(화살표 모양)을 그린다.
    void connector(float x, float iconY, GLubyte alpha = 210) {
        auto segment = [this, alpha](CCPoint from, CCPoint to, float width) {
            auto delta = to - from;
            float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            auto* line = CCLayerColor::create(ccc4(255, 217, 72, alpha), width, length);
            line->ignoreAnchorPointForPosition(false);
            line->setAnchorPoint({.5f, 0.f});
            line->setPosition(from);
            line->setRotation(-std::atan2(delta.x, delta.y) * 180.f / 3.14159265f);
            line->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
            m_players->addChild(line, -1);
        };
        segment({x, 27.f}, {x, iconY - 10.f}, .65f);
        segment({x, 27.f}, {x - 2.5f, 31.f}, .8f);
        segment({x, 27.f}, {x + 2.5f, 31.f}, .8f);
    }

public:
    // 보드 배경, 탐색 범위 띠, 레이팅 축과 눈금 라벨, 레이어(플레이어/말풍선/터치), 캡션,
    // 듀얼 수락 토글과 관리(톱니) 버튼을 만든다.
    bool init(CCSize size) {
        if (!CCNode::init()) return false;
        m_size = size;
        setContentSize(size);
        rankedPanel(this, {}, size);
        m_range = CCLayerColor::create(ccc4(255, 255, 255, 128), 1.f, 16.f);
        m_range->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
        m_range->setVisible(false);
        addChild(m_range, 1);
        m_axis = CCNode::create();
        addChild(m_axis, 2);
        auto* axis = CCDrawNode::create();
        m_axis->addChild(axis);
        axis->drawSegment({14.f, 25.f}, {size.width - 14.f, 25.f}, 1.f, {.6f, .6f, .65f, 1.f});
        for (int i = 0; i <= 5; ++i) {
            float x = ranked::ratingAxisX(size.width, i * 500.);
            axis->drawSegment({x, 22.f}, {x, 28.f}, .7f, {.8f, .8f, .85f, 1.f});
            m_ticks[i] = label(m_axis, fmt::format("{:.0f}", ranked::BOARD_RATING_MAX * i / 5), {x, 11.f}, .27f, 42.f,
                               kIce, false, "bigFont.fnt");
        }
        m_players = CCNode::create();
        addChild(m_players, 3);
        m_bubbles = CCNode::create();
        addChild(m_bubbles, 6);
        m_hits = CCMenu::create();
        m_hits->setPosition({0.f, 0.f});
        addChild(m_hits, 4);
        m_caption = label(this, "LIVE MATCHMAKING", {10.f, size.height - 13.f}, .4f, size.width - 140.f,
                          ccc3(255, 190, 110), true, "gjFont46.fnt");
        m_caption->setID("ranked-live-matchmaking"_spr);
        m_options = CCMenu::create();
        m_options->setPosition({0.f, 0.f});
        addChild(m_options, 5);
        m_duelToggle =
            CCMenuItemToggler::createWithStandardSprites(this, menu_selector(RankedSearchBoard::onDuelToggle), .42f);
        m_duelToggle->setPosition({size.width - 39.f, size.height - 13.f});
        m_duelToggle->toggle(true);
        m_duelToggle->setID("ranked-duel-toggle"_spr);
        m_options->addChild(m_duelToggle);
        m_duelLabel = label(this, "DUEL REQUESTS", {size.width - 84.f, size.height - 13.f}, .22f, 70.f, kIce, false,
                            "bigFont.fnt");
        auto* gearArea = CCNode::create();
        gearArea->setContentSize({24.f, 20.f});
        if (auto* gear = loadedFrameSprite("GJ_optionsBtn_001.png", "GJ_GameSheet03.plist")) {
            gear->setScale(13.f / std::max(1.f, gear->getContentSize().height));
            gear->setPosition({12.f, 10.f});
            gearArea->addChild(gear);
        }
        auto* manage = CCMenuItemSpriteExtra::create(gearArea, this, menu_selector(RankedSearchBoard::onManageDuels));
        manage->setPosition({size.width - 15.f, size.height - 13.f});
        manage->setID("ranked-duel-manage-button"_spr);
        m_options->addChild(manage);
        return true;
    }
    // 상태가 서버에서 새로 와서 바뀔 때 부를 콜백을 등록한다.
    void onState(std::function<void(RankedState)> callback) { m_onState = std::move(callback); }
    // Season 2 표시용 상태: 축 대신 쓰는 티어 컷오프 눈금과 보이는 레이팅 구간.
    bool m_s2 = false;
    CCNode* m_s2Cutoffs = nullptr;
    std::vector<std::pair<int, std::string>> m_s2Tiers;
    // Season 2 룩으로 전환한다: 레이팅 축과 듀얼 옵션을 숨기고 컷오프 눈금 레이어를 만든다.
    void useSeason2Look() {
        m_s2 = true;
        m_axis->setVisible(false);
        m_caption->setPosition({10.f, m_size.height - 11.f});
        m_caption->limitLabelWidth(m_size.width * .3f, .34f, .18f);
        m_s2Cutoffs = CCNode::create();
        addChild(m_s2Cutoffs, 2);
        m_options->setVisible(false);
        if (m_duelLabel) m_duelLabel->setVisible(false);
    }
    // 컵 종류에 따라 내 티어 아래/위로 몇 단계를 보일지 정하고, 티어 컷오프 목록을 받아 구간을 다시 계산한다.
    void setSeason2Tiers(Season2Tiers const& tiers) {
        m_s2Below = tiers.cup == "demon" ? 4 : 3;
        m_s2Above = tiers.cup == "demon" ? 5 : 4;
        m_s2Tiers.clear();
        for (auto const& tier : tiers.tiers) m_s2Tiers.emplace_back(tier.minRating, tier.tier);
        std::sort(m_s2Tiers.begin(), m_s2Tiers.end());
        m_s2Index = -2;
        season2Window(m_s2Rating);
    }
    double m_s2Low = 0., m_s2High = 20000., m_s2Rating = -1.;
    int m_s2Below = 3, m_s2Above = 4;
    int m_s2Index = -2;
    // Season 2 구간 안에서 레이팅을 x 좌표로 바꾼다.
    float season2X(double rating) const {
        double const t = std::clamp((rating - m_s2Low) / std::max(1., m_s2High - m_s2Low), 0., 1.);
        return 14.f + std::max(0.f, m_size.width - 28.f) * static_cast<float>(t);
    }
    // 내 레이팅이 속한 티어를 기준으로 가로축에 보일 구간(아래 몇 단계 ~ 위 몇 단계)을 정한다.
    // 가장 높은 티어 근처면 오른쪽에 여유를 더한다. 구간이 바뀌었을 때만 눈금을 다시 만든다.
    void season2Window(double rating) {
        m_s2Rating = rating;
        int const count = static_cast<int>(m_s2Tiers.size());
        if (!count) return;
        int index = 0;
        for (int i = 0; i < count; ++i)
            if (rating >= m_s2Tiers[i].first) index = i;
        if (index == m_s2Index) return;
        m_s2Index = index;
        int const low = std::max(0, index - m_s2Below), high = std::min(count - 1, index + m_s2Above);
        double from = m_s2Tiers[low].first, to = m_s2Tiers[high].first;
        if (index + m_s2Above > count - 1) {
            double const gap = count > 1 ? m_s2Tiers[count - 1].first - m_s2Tiers[count - 2].first : 1000.;
            to = std::max(to + std::max(gap, 500.), rating + 250.);
        }
        double const pad = std::max(1., to - from) * .05;
        m_s2Low = from - pad;
        m_s2High = to + pad;
        buildSeason2Cutoffs();
        m_signature.clear();
    }
    // 구간 안 티어 컷오프마다 색 눈금과 레이팅 숫자를 그린다. 라벨이 겹치면 건너뛰고,
    // 양 끝 눈금에는 티어 얼굴 아이콘을 얹는다.
    void buildSeason2Cutoffs() {
        if (!m_s2Cutoffs) return;
        m_s2Cutoffs->removeAllChildren();
        float const axisY = 18.f;
        auto* line = CCLayerColor::create(ccc4(200, 200, 208, 140), std::max(1.f, m_size.width - 28.f), 1.f);
        line->setPosition({14.f, axisY - .5f});
        m_s2Cutoffs->addChild(line, 0);
        std::vector<std::pair<int, std::string>> shown;
        for (auto const& tier : m_s2Tiers)
            if (tier.first >= m_s2Low && tier.first <= m_s2High) shown.push_back(tier);
        float lastLabel = -1000.f;
        float const lastX = shown.empty() ? 0.f : season2X(shown.back().first);
        for (size_t i = 0; i < shown.size(); ++i) {
            auto const& [rating, look] = shown[i];
            float const x = season2X(rating);
            auto const colour = rankedPalette(look).front();
            auto* tick = CCLayerColor::create(ccc4(colour.r, colour.g, colour.b, 235), 1.5f, 14.f);
            tick->setPosition({x - .75f, axisY - 6.f});
            m_s2Cutoffs->addChild(tick, 1);
            bool const end = i == 0 || i + 1 == shown.size();
            if (!end && (x - lastLabel < 26.f || lastX - x < 34.f)) continue;
            lastLabel = x;
            label(m_s2Cutoffs, std::to_string(rating), {x, axisY - 11.f}, .17f, 40.f, art::light(colour, .35f), false,
                  "bigFont.fnt");
            if (!end) continue;
            constexpr float kEndFace = 9.9f, kTickTop = 8.f;
            auto* face = season2Face(m_s2Cutoffs, look, {}, .2f);
            if (float const tall = std::max(face->getContentSize().width, face->getContentSize().height); tall > 1.f)
                face->setScale(kEndFace / tall);
            float const gap = rankedFrame(look) ? 4.f : 2.f;
            face->setPosition({x, axisY + kTickTop + gap + kEndFace / 2.f});
        }
    }
    // 축의 최댓값이 달라지면 눈금 숫자를 다시 쓴다.
    void relabelAxis(double axisMax) {
        m_axisMax = axisMax;
        for (int i = 0; i <= 5; ++i)
            if (m_ticks[i]) m_ticks[i]->setString(fmt::format("{:.0f}", m_axisMax * i / 5).c_str());
    }
    // 서버 상태로 보드를 갱신한다. 매 프레임: 탐색 범위 띠를 부드럽게 늘리고(Season 1은 대기 시간에
    // 따른 matchmakingRange로 계산), 듀얼 토글을 맞춘다. 0.2초마다: 로스터가 바뀌었는지
    // 시그니처 문자열로 비교해 바뀐 경우에만 아바타를 다시 놓는다.
    void refresh(RankedState const& state, float dt, uint64_t revision = 0) {
        bool const season2 = state.season == 2;
        if (season2) {
            m_options->setVisible(false);
            if (m_duelLabel) m_duelLabel->setVisible(false);
            double const axisMax = state.ratingMax > 0 ? state.ratingMax : ranked::BOARD_RATING_MAX;
            if (m_s2)
                season2Window(state.profile.placed ? state.profile.rating : -1.);
            else if (axisMax != m_axisMax)
                relabelAxis(axisMax);
        }
        if (m_duelToggle && !m_duelPending && m_duelToggle->isToggled() != state.allowDuel)
            m_duelToggle->toggle(state.allowDuel);
        float target = static_cast<float>(state.searchRange);
        if (state.queuedAt > 0 && !season2) {
            auto elapsed = Service::get().serverNow() - state.queuedAt;
            target =
                static_cast<float>(ranked::matchmakingRange(elapsed)) * static_cast<float>(state.searchScale) / 100.f;
        }
        m_halfRange += (target - m_halfRange) * std::min(1.f, dt * 5.f);
        auto xAt = [this](double rating) {
            return m_s2 ? season2X(rating) : ranked::ratingAxisX(m_size.width, rating * 2500 / m_axisMax);
        };
        m_axis->setVisible(state.profile.placed && !m_s2);
        float y = m_s2 ? 18.f : 25.f;
        m_range->setVisible(state.queuedAt > 0 && state.profile.placed);
        if (state.queuedAt > 0 && state.profile.placed) {
            float left = xAt(state.profile.rating - m_halfRange), right = xAt(state.profile.rating + m_halfRange);
            m_range->setPosition({left, y - 8.f});
            m_range->setContentSize({std::max(0.f, right - left), 16.f});
            m_range->setOpacity(128);
        }
        m_rosterTimer -= dt;
        if (m_rosterTimer > 0.f) return;
        m_rosterTimer = .2f;
        auto const& profile = Service::get().profile();
        std::string signature = fmt::format("{}:{}:{}:{}:{}:{}:{}:{}", state.queuedAt, state.profile.rating,
                                            state.searchingPlayers, profile.cosmetics.start, profile.cosmetics.end,
                                            profile.cosmetics.angle, profile.cosmetics.aura, state.profile.globalRank);
        signature += fmt::format(":{}:{}", state.profile.placed, state.profile.placementCompleted);
        if (revision == 0 || revision != m_boardRevision || m_boardKey.empty()) {
            m_boardRevision = revision;
            std::string key;
            for (auto const& entry : state.searchers)
                key +=
                    fmt::format("|{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}", entry.player.uid, entry.rating,
                                entry.player.name, entry.player.icon, entry.player.color1, entry.player.color2,
                                entry.player.cosmetics.start, entry.player.cosmetics.end, entry.player.cosmetics.angle,
                                entry.player.cosmetics.aura, entry.player.membership, entry.globalRank);
            for (auto const& entry : state.searchers)
                key += std::string(entry.allowDuel ? "d" : "-") + (entry.searching ? "s" : "v");
            for (auto const& entry : state.searchers) key += entry.duelAnonymous ? "a" : "n";
            key += state.match ? "m" : "";
            m_boardKey = std::move(key);
        }
        signature += m_boardKey;
        if (signature == m_signature) {
            showEmotes(state);
            return;
        }
        m_signature = std::move(signature);
        m_avatars.detach();
        m_players->removeAllChildren();
        m_hits->removeAllChildren();
        m_hitProfiles.clear();
        m_hitAnonymous.clear();
        m_iconAt.clear();
        auto own = Service::get().profile();
        // 배치를 끝내지 않았으면 레이팅 위치 대신 진행도(PLACEMENT n / m)와 내 아바타만 보인다.
        if (!state.profile.placed) {
            m_iconAt[own.uid] = CCPoint(m_size.width / 2.f, 48.f);
            bool const low = season2 && m_size.height < 120.f;
            label(m_players,
                  fmt::format("PLACEMENT  {} / {}", state.profile.placementCompleted,
                              state.profile.placementRequired > 0 ? state.profile.placementRequired : 5),
                  low ? CCPoint{m_size.width / 2.f + 24.f, m_s2 ? 40.f : 48.f}
                      : CCPoint{m_size.width / 2.f, m_size.height - 42.f},
                  low ? .3f : .38f, low ? m_size.width / 2.f - 44.f : m_size.width - 24.f, kIce, low);
            if (m_s2)
                avatar(own, "own", 0, {m_size.width / 2.f, 40.f}, .43f, 12.f, .18f, 62.f);
            else
                avatar(own, "placement", 0, {m_size.width / 2.f, 48.f}, .65f, -23.f, .3f, m_size.width - 24.f);
            if (!m_s2)
                label(m_players, "Rating ----     Tier ----", {m_size.width / 2.f, 9.f}, .22f, m_size.width - 24.f,
                      kMuted);
            m_avatars.keep();
            showEmotes(state);
            return;
        }
        float ownX = xAt(state.profile.rating);
        m_iconAt[own.uid] = CCPoint(ownX, y + 22.f);
        if (m_s2)
            avatar(own, "own", state.profile.globalRank, {ownX, y + 22.f}, .43f, 12.f, .18f, 62.f);
        else
            avatar(own, "own", state.profile.globalRank, {ownX, y + 22.f}, .43f, 14.f, .18f, 62.f);
        if (!m_s2) connector(ownX, y + 22.f);
        // 다른 탐색자는 레이팅 순으로 놓되, 가까운 아바타가 겹치지 않게 최대 3줄(lane)에 나눈다.
        // 자리가 없는 사람은 "+N searching"으로 센다.
        std::vector<std::vector<float>> occupied(3);
        occupied[0].push_back(ownX);
        int visible = 0;
        auto sorted = state.searchers;
        std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b) {
            return a.rating < b.rating || (a.rating == b.rating && a.player.uid < b.player.uid);
        });
        for (auto const& entry : sorted) {
            if (entry.player.uid == own.uid || !entry.placed) continue;
            float x = xAt(entry.rating);
            int lane = 0;
            while (lane < 3 && std::any_of(occupied[lane].begin(), occupied[lane].end(),
                                           [x](float other) { return std::abs(x - other) < 64.f; }))
                ++lane;
            if (lane == 3) continue;
            float rowY = y + 22.f + lane * 40.f;
            if (m_s2 ? lane > 0 : rowY + 16.f > m_size.height - 26.f) continue;
            occupied[lane].push_back(x);
            m_iconAt[entry.player.uid] = CCPoint(x, rowY);
            if (!m_s2) connector(x, rowY, entry.searching ? 210 : 80);
            auto* holder = avatar(entry.player, entry.searching ? "searching" : "viewing", entry.globalRank, {x, rowY},
                                  .32f, m_s2 ? 11.f : 13.f, .18f, 62.f);
            if (!entry.searching && holder->getTag() != 110) {
                previewOpacity(holder, 110);
                holder->setTag(110);
            }
            addHit(entry.player, {x, rowY + 4.f});
            // 듀얼 버튼: 매치 중이 아니고, 상대가 듀얼을 받으며, 레이팅 조건(최소 레이팅, 레이팅 차이)을 만족할 때만 둔다.
            if (!state.match && entry.allowDuel && ranked::duelTargetAllowed(entry.rating)) {
                if (ranked::duelChallengerAllowed(state.profile.rating, entry.rating)) {
                    bool const room = rowY + 34.f <= m_size.height - 20.f;
                    addDuel(entry.player, room ? CCPoint{x, rowY + 25.f} : CCPoint{x + 21.f, rowY - 2.f},
                            entry.duelAnonymous);
                }
            }
            ++visible;
        }
        int others = std::max(0, state.searchingPlayers - (state.queuedAt > 0 ? 1 : 0) - visible);
        if (others && m_s2) {
            auto* more = label(m_players, fmt::format("+{} searching", others),
                               {m_size.width - 3.f, m_size.height - 19.f}, .26f, 90.f, kMuted, false, "chatFont.fnt");
            more->setAnchorPoint({1.f, .5f});
        } else if (others)
            label(m_players, fmt::format("+{} searching", others), {m_size.width - 45.f, m_size.height - 30.f}, .16f,
                  80.f, kMuted);
        m_avatars.keep();
        showEmotes(state);
    }
    // 아바타(아이콘 + 닉네임) 노드를 만든다. 같은 내용이면 캐시(m_avatars)에서 재사용한다.
    CCNode* avatar(PlayerProfile const& profile, char const* kind, int rank, CCPoint at, float iconScale,
                   float nameOffset, float nameScale, float nameWidth) {
        auto key = fmt::format("{}|{}|{}|{}", kind, rank, nameWidth, avatarKey(profile));
        auto* holder = m_avatars.take(profile.uid, key);
        if (!holder) {
            holder = CCNode::create();
            player(holder, profile, {0.f, 0.f}, iconScale);
            rankedNickname(holder, profile, rank, {0.f, nameOffset}, nameScale, nameWidth);
        }
        holder->setPosition(at);
        m_players->addChild(holder);
        m_avatars.put(profile.uid, std::move(key), holder);
        return holder;
    }
    // 내가 직접 보낸 이모트를 즉시 내 아이콘 위에 띄운다(서버 상태에서 다시 뜨지 않도록 시각을 기록).
    void localEmote(std::string const& kind) {
        m_localEmoteAt = Service::get().serverNow();
        popEmote(Service::get().profile().uid, kind);
    }
    static RankedSearchBoard* create(CCSize size) {
        auto* result = new RankedSearchBoard;
        if (result->init(size)) {
            result->autorelease();
            return result;
        }
        delete result;
        return nullptr;
    }
};
