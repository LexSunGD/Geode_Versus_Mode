// Season 2 plaza(lounge 장면) 안의 다른 플레이어 표시 (VersusUI.cpp에 #include되는 조각).
// plazaBubble: 머리 위 말풍선 / 이모트, PlazaCrowd: 서버와 위치를 주고받으며 다른 플레이어를
// 부드럽게 재생하는 군중 노드, Season2Billboard: 내 레이팅 위치를 보여 주는 안내판이다.
// 채팅 / 이모트 말풍선을 holder 위에 잠깐 띄운다. kind가 "chat"이면 text, 아니면 이모트 ID.
void plazaBubble(CCNode* holder, std::string const& kind, std::string const& text) {
    if (!holder) return;
    holder->removeAllChildren();
    bool const chat = kind == "chat";
    if (!chat && !emotes::find(kind)) return;
    auto* bubble = CCNodeRGBA::create();
    bubble->setCascadeOpacityEnabled(true);
    bubble->setScale(.15f);
    holder->addChild(bubble);
    auto* bg = NineSlice::create("square02b_001.png");
    bubble->addChild(bg);
    if (chat) {
        auto* words = label(bubble, text, {0.f, 1.f}, .4f, 110.f, ccc3(22, 35, 55), false, "bigFont.fnt");
        bg->setContentSize({std::max(40.f, words->getScaledContentSize().width + 16.f), 26.f});
    } else {
        bg->setContentSize({30.f, 28.f});
        auto* art = emoteArt(kind);
        art->setScale(.58f);
        bubble->addChild(art, 2);
        emoteBurst(holder, {}, kind, .62f);
        art->runAction(
            CCRepeat::create(CCSequence::create(CCRotateTo::create(.12f, -9.f), CCRotateTo::create(.12f, 9.f),
                                                CCRotateTo::create(.12f, 0.f), nullptr),
                             2));
    }
    bubble->runAction(CCSequence::create(CCScaleTo::create(.12f, 1.f), CCDelayTime::create(chat ? 2.4f : 1.6f),
                                         CCFadeOut::create(.25f), CCRemoveSelf::create(), nullptr));
}

// plaza의 다른 플레이어(Walker) 군중. 내 위치를 0.1초 간격으로 샘플링해 서버로 보내고,
// 응답으로 받은 다른 플레이어의 샘플을 일정 지연(lag)을 두고 보간하며 재생한다.
// 좌표 샘플은 레벨 좌표 x10의 정수 값이고, 샘플 번호는 서버 시각 기준 0.1초 단위 인덱스다.
class PlazaCrowd final : public CCNode {
    // kSampleSeconds: 샘플 간격(초), kMaxSent: 한 번에 보낼 샘플 수 기본 상한, kKept: 플레이어당 보관 샘플 수.
    // kSettle: 같은 자리에 이만큼 쌓이면 샘플 전송을 멈춤. kLag*: 재생 지연 범위(샘플 수).
    // kCoast: 샘플이 모자랄 때 마지막 속도로 더 나아갈 수 있는 샘플 수. kEase: 화면 위치 추종 속도.
    // kSnap: 이 거리(x10 단위) 넘게 어긋나면 보간 없이 즉시 이동. kFade*: 가까운 플레이어를 흐리게 하는 거리 범위.
    static constexpr float kSampleSeconds = .1f;
    static constexpr size_t kMaxSent = 10;
    static constexpr size_t kKept = 80;
    static constexpr int kSettle = 12;
    static constexpr float kLagMin = 3.f, kLagMax = 20.f;
    static constexpr float kCoast = 4.f;
    static constexpr float kEase = 22.f;
    static constexpr int kSnap = 2000;
    static constexpr float kFadeNear = 7.5f, kFadeFar = 45.f;
    static constexpr GLubyte kFadeMin = 0;
    // 파티클 시스템과 원래 알파값 (페이드 시 원본을 기준으로 다시 계산하기 위함).
    struct Particles {
        Ref<CCParticleSystem> system;
        float start = 1.f, startVar = 0.f, end = 1.f, endVar = 0.f;
    };
    // 다른 플레이어 한 명의 표시 상태: 노드(아이콘, 말풍선), 수신한 샘플 버퍼와 재생 위치(head),
    // 재생 지연(lag) 조절용 값, 실제 그려지는 위치(drawn*)와 투명도.
    struct Walker {
        CCNode* root = nullptr;
        CCNode* icon = nullptr;
        CCNode* bubbles = nullptr;
        std::string key;
        std::string uid;
        PlayerProfile player;
        std::deque<PlazaSample> samples;
        int64_t base = 0;
        double head = -1.;
        bool waiting = false;
        float quiet = 0.f;
        float lag = -1.f;
        bool dry = false;
        float steady = 0.f, lowest = 1e9f;
        bool shown = false;
        float drawnX = 0.f, drawnY = 0.f, drawnRotation = 0.f;
        bool drawnLeft = false;
        int64_t eventSeen = 0;
        GLubyte alpha = 255;
        std::vector<Particles> particles;
    };
    // m_host: lounge 호스트, m_walkers: 서버가 준 ID별 플레이어.
    // m_delay / m_period: 서버가 알려 준 갱신 주기에 맞춘 재생 지연(샘플 수)과 전송 주기(초).
    // m_pending / m_pendingFirst / m_index: 아직 보내지 않은 내 샘플과 그 번호. m_since: 서버에서 마지막으로 받은 tick.
    // m_epoch: 서버 세션 구분값(바뀌면 전부 초기화). m_partner: 지정되면 그 플레이어만 보임.
    Weak<CCNode> m_host;
    std::map<int, Walker> m_walkers;
    float m_delay = 2.5f;
    float m_period = .1f;
    float m_sampleClock = 0.f, m_wait = 0.f, m_flight = 0.f;
    int64_t m_index = 0, m_since = 0, m_epoch = 0;
    std::deque<PlazaSample> m_pending;
    int64_t m_pendingFirst = 0;
    std::optional<PlazaSample> m_last;
    size_t m_maxSent = kMaxSent;
    int m_still = 0;
    std::optional<lounge::Pose> m_frame;
    std::optional<lounge::Pose> m_me;
    bool m_inFlight = false, m_needProfile = true, m_active = false;
    int m_failures = 0;
    int m_generation = 0;
    std::string m_eventKind, m_eventText;
    std::string m_partner;
    bool m_hidden = false;
    static constexpr int kJump = 2000;

    // 외형이 바뀌었는지 판단하는 키 (아바타 + 티어).
    static std::string walkerKey(RankedProfile const& info) {
        return avatarKey(info.player) + "|" + (info.placed ? info.tier : std::string("Unranked"));
    }
    // 플레이어 노드를 새로 만든다: 아이콘, 닉네임, 티어 얼굴, 타이틀 배지, 말풍선 자리.
    void build(Walker& walker, RankedProfile const& info) {
        if (walker.root) walker.root->removeFromParent();
        walker.key = walkerKey(info);
        walker.player = info.player;
        walker.root = CCNode::create();
        walker.root->setVisible(false);
        addChild(walker.root);
        walker.icon = player(walker.root, info.player, {}, 1.f);
        rankedNickname(walker.root, info.player, 0, {0.f, -23.f}, .3f, 110.f);
        auto* face = season2Face(walker.root, info.placed ? info.tier : std::string("Unranked"), {0.f, 27.f}, .3f);
        if (float const tall = std::max(face->getContentSize().width, face->getContentSize().height); tall > 1.f)
            face->setScale(16.f / tall);
        titleBadge(walker.root, info.player.cosmetics.title, {0.f, 45.f}, .85f, 160.f);
        walker.bubbles = CCNode::create();
        walker.bubbles->setPosition({0.f, 66.f});
        walker.root->addChild(walker.bubbles, 10);
        walker.alpha = 255;
        walker.particles.clear();
        collectParticles(walker.root, walker.bubbles, walker.particles);
    }
    // 페이드 때 알파를 따로 조절해야 하는 파티클 시스템을 모은다 (말풍선 제외).
    static void collectParticles(CCNode* node, CCNode* skip, std::vector<Particles>& out) {
        for (auto* child : CCArrayExt<CCNode*>(node->getChildren())) {
            if (child == skip) continue;
            if (auto* system = typeinfo_cast<CCParticleSystem*>(child))
                out.push_back({system, system->getStartColor().a, system->getStartColorVar().a, system->getEndColor().a,
                               system->getEndColorVar().a});
            collectParticles(child, skip, out);
        }
    }
    // 자식 노드에 투명도를 일괄 적용한다 (말풍선 제외). cascade 불투명도를 쓰는 노드는 그 값을 아래로 전달.
    static void fadeTree(CCNode* node, GLubyte inherited, CCNode* skip) {
        for (auto* child : CCArrayExt<CCNode*>(node->getChildren())) {
            if (child == skip) continue;
            GLubyte next = inherited;
            if (auto* rgba = typeinfo_cast<CCRGBAProtocol*>(child)) {
                rgba->updateDisplayedOpacity(inherited);
                if (rgba->isCascadeOpacityEnabled()) next = rgba->getDisplayedOpacity();
            }
            fadeTree(child, next, skip);
        }
    }
    // 파티클의 시작 / 끝 색 알파에 투명도 비율을 곱한다.
    static void fadeParticles(Walker& walker, GLubyte alpha) {
        float const k = alpha / 255.f;
        for (auto& each : walker.particles) {
            if (!each.system) continue;
            auto colour = each.system->getStartColor();
            colour.a = each.start * k;
            each.system->setStartColor(colour);
            colour = each.system->getStartColorVar();
            colour.a = each.startVar * k;
            each.system->setStartColorVar(colour);
            colour = each.system->getEndColor();
            colour.a = each.end * k;
            each.system->setEndColor(colour);
            colour = each.system->getEndColorVar();
            colour.a = each.endVar * k;
            each.system->setEndColorVar(colour);
        }
    }
    // 내 위치와의 거리에 따른 투명도. 가까울수록(kFadeNear 이내) 투명하고 멀수록(kFadeFar 이상) 불투명하다.
    GLubyte fadeFor(Walker const& walker) const {
        if (!m_me) return 255;
        float const dx = walker.drawnX / 10.f - m_me->x, dy = walker.drawnY / 10.f - m_me->y;
        float const t = std::clamp((std::sqrt(dx * dx + dy * dy) - kFadeNear) / (kFadeFar - kFadeNear), 0.f, 1.f);
        return static_cast<GLubyte>(std::lround(kFadeMin + (255.f - kFadeMin) * t));
    }
    // 서버에서 받은 샘플(first 번호부터)을 버퍼에 합친다. 이미 있는 번호는 건너뛰고,
    // 번호가 크게 건너뛰거나 되돌아가면 버퍼를 비우고 새로 시작한다. 빈 번호는 직전 샘플로 채우며 kKept개만 남긴다.
    static void add(Walker& walker, int64_t first, std::vector<PlazaSample> const& samples) {
        for (size_t i = 0; i < samples.size(); ++i) {
            int64_t const index = first + static_cast<int64_t>(i);
            int64_t const next = walker.base + static_cast<int64_t>(walker.samples.size());
            if (!walker.samples.empty() && index < next && index >= next - static_cast<int64_t>(kKept)) continue;
            if (walker.samples.empty() || index > next + 40 || index < next) {
                walker.samples.clear();
                walker.base = index;
                walker.head = -1.;
                walker.samples.push_back(samples[i]);
                continue;
            }
            while (walker.base + static_cast<int64_t>(walker.samples.size()) < index)
                walker.samples.push_back(walker.samples.back());
            walker.samples.push_back(samples[i]);
        }
        while (walker.samples.size() > kKept) {
            walker.samples.pop_front();
            ++walker.base;
        }
        if (walker.head >= 0. && walker.head < static_cast<double>(walker.base))
            walker.head = static_cast<double>(walker.base);
    }
    // 서버 응답을 반영한다. epoch가 바뀌면 전부 초기화 후 프로필부터 다시 받는다.
    // 전송 주기 / 지연을 서버 값에 맞추고, 목록에서 사라진 플레이어는 제거, 변경된 플레이어는 (필요하면 노드를
    // 새로 만들고) 샘플과 이모트 / 채팅 이벤트를 반영한다. 정보를 모르는 ID가 있으면 since를 0으로 돌려 전체를 다시 받는다.
    void apply(PlazaAnswer const& answer) {
        if (answer.epoch != m_epoch) {
            bool const first = m_epoch == 0;
            m_epoch = answer.epoch;
            if (!first) {
                for (auto& [id, walker] : m_walkers)
                    if (walker.root) walker.root->removeFromParent();
                m_walkers.clear();
                m_since = 0;
                m_needProfile = true;
                m_wait = 0.f;
                return;
            }
        }
        m_since = answer.tick;
        m_needProfile = answer.needProfile;
        m_maxSent = static_cast<size_t>(std::max(1, answer.maxSamples));
        m_delay = std::clamp((answer.everyMs + 300.f) / (kSampleSeconds * 1000.f) + 1.f, 4.f, 20.f);
        m_period = std::clamp(answer.everyMs / 1000.f, kSampleSeconds, 5.f);
        m_wait = std::max(0.f, answer.everyMs / 1000.f - m_flight);
        for (auto it = m_walkers.begin(); it != m_walkers.end();) {
            if (std::find(answer.ids.begin(), answer.ids.end(), it->first) == answer.ids.end()) {
                if (it->second.root) it->second.root->removeFromParent();
                it = m_walkers.erase(it);
            } else
                ++it;
        }
        bool unknown = false;
        for (auto const& change : answer.changes) {
            if (std::find(answer.ids.begin(), answer.ids.end(), change.id) == answer.ids.end()) continue;
            auto& walker = m_walkers[change.id];
            if (change.info && change.info->player.uid != walker.uid) {
                if (walker.root) walker.root->removeFromParent();
                walker = Walker{};
                walker.uid = change.info->player.uid;
            }
            if (change.info && walkerKey(*change.info) != walker.key) build(walker, *change.info);
            if (!walker.root) {
                m_walkers.erase(change.id);
                unknown = true;
                continue;
            }
            if (!change.samples.empty()) {
                walker.quiet = 0.f;
                add(walker, change.first, change.samples);
            }
            if (change.eventNonce > walker.eventSeen) {
                walker.eventSeen = change.eventNonce;
                plazaBubble(walker.bubbles, change.eventKind, change.eventText);
            }
        }
        if (unknown) m_since = 0;
    }
    // 모아 둔 내 샘플과 이벤트(이모트 / 채팅)를 서버에 보내고 응답을 apply로 처리한다.
    // 서버가 프로필을 요구하면(needProfile) 마지막 샘플과 함께 보낸다. 실패하면 2^n초(최대 16초) 뒤에 다시 시도한다.
    void send() {
        PlazaUpdate update;
        update.since = m_since;
        update.withProfile = m_needProfile;
        if (update.withProfile && m_pending.empty() && m_last) {
            m_pendingFirst = m_index++;
            m_pending.push_back(*m_last);
        }
        update.first = m_pendingFirst;
        update.samples.assign(m_pending.begin(), m_pending.end());
        update.eventKind = std::move(m_eventKind);
        update.eventText = std::move(m_eventText);
        m_eventKind.clear();
        m_eventText.clear();
        m_pending.clear();
        m_inFlight = true;
        m_flight = 0.f;
        Service::get().exchangePlaza(std::move(update), [self = Weak<PlazaCrowd>(this), generation = m_generation](
                                                            std::optional<PlazaAnswer> answer, std::string) {
            auto owner = self.lock();
            if (!owner) return;
            owner->m_inFlight = false;
            if (!owner->m_active || generation != owner->m_generation) return;
            if (!answer) {
                owner->m_failures = std::min(owner->m_failures + 1, 4);
                owner->m_wait = static_cast<float>(1 << owner->m_failures);
                owner->m_needProfile = true;
                return;
            }
            owner->m_failures = 0;
            owner->apply(*answer);
        });
    }
    // 내 위치를 샘플로 저장한다. 같은 자리에 kSettle번 이상 있으면 더 쌓지 않고, 보낼 샘플이 상한을 넘으면 오래된 것부터 버린다.
    void sample(lounge::Pose const& pose) {
        PlazaSample const now{static_cast<int>(std::lround(pose.x * 10.f)),
                              static_cast<int>(std::lround(pose.y * 10.f)),
                              static_cast<int>(std::lround(std::fmod(pose.rotation, 360.f))), pose.left};
        if (m_last && *m_last == now) {
            if (m_still >= kSettle) return;
            ++m_still;
        } else
            m_still = 0;
        m_last = now;
        if (m_pending.empty()) m_pendingFirst = m_index;
        m_pending.push_back(now);
        ++m_index;
        while (m_pending.size() > m_maxSent) {
            m_pending.pop_front();
            ++m_pendingFirst;
        }
    }
    // 모든 상태를 초기화한다. generation을 올려 이미 보낸 요청의 응답은 무시된다.
    void clear() {
        ++m_generation;
        for (auto& [id, walker] : m_walkers)
            if (walker.root) walker.root->removeFromParent();
        m_walkers.clear();
        m_since = 0;
        m_needProfile = true;
        m_pending.clear();
        m_last.reset();
        m_still = 0;
        m_frame.reset();
        m_me.reset();
        m_eventKind.clear();
        m_eventText.clear();
    }
    // 두 샘플 사이가 순간이동에 가까울 만큼 먼지 (보간하지 않고 한쪽 값을 사용).
    static bool jump(PlazaSample const& a, PlazaSample const& b) {
        return std::abs(b.x - a.x) > kJump || std::abs(b.y - a.y) > kJump;
    }
    // turn: 각도 차이를 [-180, 180]로 정규화. moving: 마지막 두 샘플이 달라 움직이는 중인지.
    static float turn(float from, float to) { return std::remainder(to - from, 360.f); }
    static bool moving(Walker const& walker) {
        size_t const n = walker.samples.size();
        return n >= 2 && !(walker.samples[n - 1] == walker.samples[n - 2]);
    }
    // 단조 3차 에르미트 보간 (p1 -> p2, t는 0..1). 기울기를 제한해 구간 밖으로 튀지 않게 한다.
    static float monotone(float p0, float p1, float p2, float p3, float t) {
        float const d0 = p1 - p0, d1 = p2 - p1, d2 = p3 - p2;
        auto slope = [](float a, float b) {
            if (a * b <= 0.f) return 0.f;
            float const m = (a + b) / 2.f, limit = 3.f * std::min(std::abs(a), std::abs(b));
            return std::clamp(m, -limit, limit);
        };
        float const m1 = slope(d0, d1), m2 = slope(d1, d2);
        float const t2 = t * t, t3 = t2 * t;
        return (2.f * t3 - 3.f * t2 + 1.f) * p1 + (t3 - 2.f * t2 + t) * m1 + (-2.f * t3 + 3.f * t2) * p2 +
               (t3 - t2) * m2;
    }
    // 재생 위치(head)에서의 좌표 / 회전 / 방향을 계산한다. 버퍼 끝을 넘으면 마지막 속도로 조금 외삽하고,
    // 순간이동 구간이면 보간 없이 가까운 쪽 샘플을 쓴다.
    static void at(Walker const& walker, float& x, float& y, float& rotation, bool& left) {
        auto const& s = walker.samples;
        int64_t const n = static_cast<int64_t>(s.size());
        double const head = std::max(walker.head, static_cast<double>(walker.base));
        double const offset = head - static_cast<double>(walker.base);
        if (offset >= static_cast<double>(n - 1)) {
            auto const& b = s[n - 1];
            float const past = static_cast<float>(offset - static_cast<double>(n - 1));
            if (n >= 2 && past > 0.f && !jump(s[n - 2], b)) {
                auto const& a = s[n - 2];
                x = b.x + (b.x - a.x) * past;
                y = static_cast<float>(b.y);
                rotation = b.rotation + turn(static_cast<float>(a.rotation), static_cast<float>(b.rotation)) * past;
            } else {
                x = static_cast<float>(b.x);
                y = static_cast<float>(b.y);
                rotation = static_cast<float>(b.rotation);
            }
            left = b.left;
            return;
        }
        int64_t const i = static_cast<int64_t>(std::floor(offset));
        float const t = static_cast<float>(offset - static_cast<double>(i));
        auto const& a = s[i];
        auto const& b = s[i + 1];
        if (jump(a, b)) {
            auto const& now = t < .5f ? a : b;
            x = static_cast<float>(now.x);
            y = static_cast<float>(now.y);
            rotation = static_cast<float>(now.rotation);
            left = now.left;
            return;
        }
        auto const& before = i > 0 && !jump(s[i - 1], a) ? s[i - 1] : a;
        auto const& after = i + 2 < n && !jump(b, s[i + 2]) ? s[i + 2] : b;
        x = monotone(before.x, a.x, b.x, after.x, t);
        y = monotone(before.y, a.y, b.y, after.y, t);
        rotation = a.rotation + turn(static_cast<float>(a.rotation), static_cast<float>(b.rotation)) * t;
        left = t < .5f ? a.left : b.left;
    }
    // host: 이 군중을 붙일 lounge 호스트.
public:
    static PlazaCrowd* create(CCNode* host) {
        auto* crowd = new PlazaCrowd;
        if (!crowd->init()) {
            delete crowd;
            return nullptr;
        }
        crowd->autorelease();
        crowd->m_host = host;
        crowd->setID("season2-plaza"_spr);
        crowd->m_index = std::max<int64_t>(0, Service::get().serverNow() / 100);
        return crowd;
    }
    // 매 프레임 호출. active가 아니면 상태를 비운다. 켜져 있으면 내 위치를 샘플링하고
    // (프레임 사이 시간을 보간해 정확한 0.1초 시점의 좌표를 구함), 요청이 진행 중이 아니고 대기 시간이 끝났으면 send()한다.
    void tick(float dt, bool active) {
        auto host = m_host.lock();
        if (!host || !active) {
            if (m_active) {
                m_active = false;
                clear();
            }
            return;
        }
        m_active = true;
        m_flight += dt;
        lounge::Pose pose;
        if (lounge::pose(host.data(), pose)) {
            m_me = pose;
            m_sampleClock += dt;
            if (m_sampleClock >= kSampleSeconds || !m_last) {
                lounge::Pose exact = pose;
                if (m_frame && dt > 0.f && m_last) {
                    float const f = std::clamp(1.f - (m_sampleClock - kSampleSeconds) / dt, 0.f, 1.f);
                    auto const& was = *m_frame;
                    if (std::abs(pose.x - was.x) * 10.f <= kJump && std::abs(pose.y - was.y) * 10.f <= kJump) {
                        exact.x = was.x + (pose.x - was.x) * f;
                        exact.y = was.y + (pose.y - was.y) * f;
                        exact.rotation = was.rotation + turn(was.rotation, pose.rotation) * f;
                    }
                }
                m_sampleClock = std::fmod(m_sampleClock, kSampleSeconds);
                sample(exact);
            }
            m_frame = pose;
        }
        if (!m_inFlight && (m_wait -= dt) <= 0.f) send();
        for (auto& [id, walker] : m_walkers) play(walker, dt);
    }
    // 플레이어 한 명의 재생 위치를 진행시킨다. head를 newest - lag 근처로 유지하도록 재생 속도를 조금씩 조절하고,
    // 샘플이 모자라면(dry) lag을 늘려 여유를 두고, 3초 동안 계속 여유가 크면 lag을 줄인다.
    // 정지한 상대는 마지막 샘플에서 기다린다(waiting). 이후 화면 위치는 지수 감쇠로 부드럽게 따라간다.
    void play(Walker& walker, float dt) {
        if (walker.samples.empty()) return;
        double const newest = static_cast<double>(walker.base + static_cast<int64_t>(walker.samples.size()) - 1);
        walker.quiet += dt;
        if (walker.lag < 0.f) walker.lag = std::clamp(m_delay, kLagMin, kLagMax);
        float const lag = walker.lag;
        if (walker.head < 0.) walker.head = std::max(static_cast<double>(walker.base), newest - lag);
        if (newest - walker.head > lag + 15.) walker.head = newest - lag;
        float const stalled = lag * kSampleSeconds + .4f;
        bool const going = moving(walker) && walker.quiet < stalled;
        bool hold = false;
        if (walker.waiting) {
            if (going && newest - walker.head < lag)
                hold = true;
            else
                walker.waiting = false;
        }
        if (!hold) {
            float const since = std::min(walker.quiet, m_period);
            float const ahead = static_cast<float>(newest - walker.head) + (since - m_period / 2.f) / kSampleSeconds;
            float const off = ahead - lag, beyond = std::max(0.f, std::abs(off) - 1.5f);
            float const rate = 1.f + std::clamp(std::copysign(beyond, off) * .06f, -.3f, .5f);
            double const before = walker.head;
            walker.head += dt / kSampleSeconds * rate;
            if (walker.head >= newest) {
                if (going) {
                    if (!walker.dry) {
                        walker.dry = true;
                        walker.lag = std::min(kLagMax, walker.lag + 1.5f);
                        walker.steady = 0.f;
                        walker.lowest = 1e9f;
                    }
                    walker.head = std::min(walker.head, newest + kCoast);
                } else if (moving(walker)) {
                    walker.head = walker.quiet > stalled + 1.5f ? newest : std::clamp(before, newest, newest + kCoast);
                    walker.waiting = true;
                } else {
                    walker.head = newest;
                    walker.waiting = true;
                }
            } else
                walker.dry = false;
            walker.head = std::max(walker.head, static_cast<double>(walker.base));
            if (going) {
                walker.steady += dt;
                walker.lowest = std::min(walker.lowest, static_cast<float>(newest - walker.head));
                if (walker.steady >= 3.f) {
                    if (walker.lowest > 2.f) walker.lag = std::max(kLagMin, walker.lag - .5f);
                    walker.steady = 0.f;
                    walker.lowest = 1e9f;
                }
            }
        }
        float x = 0.f, y = 0.f, rotation = 0.f;
        bool left = false;
        at(walker, x, y, rotation, left);
        if (!walker.shown || std::abs(x - walker.drawnX) > kSnap || std::abs(y - walker.drawnY) > kSnap) {
            walker.shown = true;
            walker.drawnX = x;
            walker.drawnY = y;
            walker.drawnRotation = rotation;
        } else {
            float const k = 1.f - std::exp(-kEase * std::clamp(dt, 0.f, .1f));
            walker.drawnX += (x - walker.drawnX) * k;
            walker.drawnY += (y - walker.drawnY) * k;
            walker.drawnRotation += turn(walker.drawnRotation, rotation) * k;
        }
        walker.drawnLeft = left;
    }
    // 다음 프레임에 바로 샘플을 만들고 보내도록 대기 상태를 초기화한다 (위치가 크게 바뀐 직후 등).
    void nudge() {
        m_sampleClock = kSampleSeconds;
        m_still = 0;
        m_wait = 0.f;
        m_frame.reset();
    }
    // emote: 다음 전송에 실을 내 이모트 / 채팅 이벤트를 저장한다.
    void emote(std::string kind, std::string text) {
        m_eventKind = std::move(kind);
        m_eventText = std::move(text);
    }
    // setRoom: partner UID가 지정되면 해당 플레이어만 보이도록 하고 서버 상태를 다시 받는다. setHidden: 전부 숨김.
    void setRoom(std::string partner) {
        if (partner == m_partner) return;
        m_partner = std::move(partner);
        ++m_generation;
        m_since = 0;
    }
    void setHidden(bool hidden) { m_hidden = hidden; }
    // 화면 좌표 근처에 보이는 플레이어 중 가장 가까운 프로필을 반환한다 (터치 선택용). 완전히 투명하면 제외.
    std::optional<PlayerProfile> walkerAt(CCPoint screen) const {
        std::optional<PlayerProfile> found;
        float best = 0.f;
        for (auto const& [id, walker] : m_walkers) {
            if (!walker.root || !walker.root->isVisible() || !walker.icon) continue;
            if (walker.alpha == 0) continue;
            auto const center = walker.root->convertToWorldSpace(CCPointZero);
            auto const corner = walker.root->convertToWorldSpace({18.f, 18.f});
            float const reach = std::max(10.f, std::abs(corner.x - center.x));
            float const dx = screen.x - center.x, dy = screen.y - center.y;
            if (std::abs(dx) > reach || std::abs(dy) > reach) continue;
            float const distance = dx * dx + dy * dy;
            if (!found || distance < best) {
                found = walker.player;
                best = distance;
            }
        }
        return found;
    }
    // 그리기 직전에 각 플레이어 노드의 표시 여부 / 위치 / 아이콘 회전 / 좌우 반전 / 투명도를 갱신한다.
    // 숨김 상태, 샘플 없음, 다른 방 상대, 호스트 좌표로 변환 불가일 때는 숨긴다.
    void visit() override {
        auto host = m_host.lock();
        bool const inLevel = host && lounge::crowdInLevel(host.data());
        for (auto& [id, walker] : m_walkers) {
            if (!walker.root) continue;
            CCPoint place;
            if (!host || walker.samples.empty() || walker.head < 0. || !walker.shown || m_hidden ||
                (!m_partner.empty() && walker.uid != m_partner)) {
                walker.root->setVisible(false);
                continue;
            }
            if (inLevel)
                place = CCPoint{walker.drawnX / 10.f, walker.drawnY / 10.f};
            else if (!lounge::toHost(host.data(), {walker.drawnX / 10.f, walker.drawnY / 10.f}, place)) {
                walker.root->setVisible(false);
                continue;
            }
            walker.root->setVisible(true);
            walker.root->setPosition(place);
            walker.icon->setRotation(walker.drawnRotation);
            walker.icon->setScaleX(walker.drawnLeft ? -std::abs(walker.icon->getScaleY())
                                                    : std::abs(walker.icon->getScaleY()));
            GLubyte const alpha = fadeFor(walker);
            if (alpha < 255 || walker.alpha < 255) {
                fadeTree(walker.root, alpha, walker.bubbles);
                if (alpha != walker.alpha) fadeParticles(walker, alpha);
                walker.alpha = alpha;
            }
        }
        CCNode::visit();
    }
};

// "MY CURRENT POSITION" 안내판: 레이팅 축 위에 티어 경계를 눈금으로 두고
// 현재 레이팅 위치에 내 아이콘과 점수를 표시한다. 표시 구간은 현재 티어 주변 몇 개 티어로 한정된다.
class Season2Billboard final : public CCNode {
    // 안내판 크기와 배치 상수. m_below / m_above: 현재 티어 아래 / 위로 보여 줄 티어 수.
    // m_rating이 음수면 배치 중(-1)이거나 아직 값 없음(-2)이다.
    static constexpr float kW = 240.f, kH = 60.f, kAxisY = 19.f, kSide = 16.f;
    static constexpr float kIconScale = .36f, kIconHalf = 15.f * kIconScale;
    static constexpr float kCornerY = kH - 6.5f;
    std::vector<std::pair<int, std::string>> m_tiers;
    int m_below = 2, m_above = 3;
    double m_low = 0., m_high = 20000.;
    double m_rating = -2.;
    std::string m_look;
    int m_index = -3;
    CCNode* m_axis = nullptr;
    CCNode* m_marker = nullptr;
    CCLabelBMFont* m_cup = nullptr;
    Ref<CCNode> m_avatar;
    std::string m_avatarKey;
    // 내 아바타가 바뀌었는지, 바뀌었으면 아이콘 노드를 새로 만든다.
    bool avatarChanged() const { return !m_avatar || avatarKey(Service::get().profile()) != m_avatarKey; }
    CCNode* avatar() {
        if (avatarChanged()) {
            auto const own = Service::get().profile();
            if (m_avatar) m_avatar->removeFromParent();
            auto* holder = CCNode::create();
            holder->setID("season2-billboard-icon"_spr);
            player(holder, own, {0.f, 0.f}, kIconScale);
            m_avatar = holder;
            m_avatarKey = avatarKey(own);
        }
        return m_avatar.data();
    }
    void showAvatar(CCPoint at) {
        auto* icon = avatar();
        icon->setPosition(at);
        m_marker->addChild(icon, 2);
    }
    // 레이팅 -> 축 위 x 좌표.
    float xAt(double rating) const {
        double const t = std::clamp((rating - m_low) / std::max(1., m_high - m_low), 0., 1.);
        return kSide + (kW - 2.f * kSide) * static_cast<float>(t);
    }
    // 배경, 테두리, 제목 라벨, 축 / 마커 노드를 만든다.
    bool init() override {
        if (!CCNode::init()) return false;
        setID("season2-rank-billboard"_spr);
        setContentSize({kW, kH});
        setAnchorPoint({.5f, .5f});
        ignoreAnchorPointForPosition(false);
        auto* back = NineSlice::create("square02b_001.png");
        back->setContentSize({kW, kH});
        back->setColor(ccc3(8, 10, 22));
        back->setOpacity(225);
        back->setPosition({kW / 2.f, kH / 2.f});
        addChild(back, 0);
        auto* rim = NineSlice::create("square02b_001.png");
        rim->setContentSize({kW + 3.f, kH + 3.f});
        rim->setColor(ccc3(120, 170, 255));
        rim->setOpacity(70);
        rim->setPosition({kW / 2.f, kH / 2.f});
        addChild(rim, -1);
        label(this, "MY CURRENT POSITION", {8.f, kCornerY}, .2f, kW * .5f, kIce, true, "bigFont.fnt");
        m_cup = label(this, "", {kW - 8.f, kCornerY}, .2f, kW * .4f, ccc3(255, 214, 120), false, "goldFont.fnt");
        m_cup->setAnchorPoint({1.f, .5f});
        m_axis = CCNode::create();
        addChild(m_axis, 2);
        m_marker = CCNode::create();
        addChild(m_marker, 4);
        return true;
    }
    // 현재 레이팅이 속한 티어를 찾아 보여 줄 레이팅 구간을 정한다 (최상위 근처면 위쪽 여유를 둠).
    // 티어 인덱스가 같으면 다시 만들지 않는다.
    void window() {
        int const count = static_cast<int>(m_tiers.size());
        if (!count) return;
        double const rating = std::max(0., m_rating);
        int index = 0;
        for (int i = 0; i < count; ++i)
            if (rating >= m_tiers[i].first) index = i;
        if (index == m_index) return;
        m_index = index;
        int const low = std::max(0, index - m_below), high = std::min(count - 1, index + m_above);
        double from = m_tiers[low].first, to = m_tiers[high].first;
        if (index + m_above > count - 1) {
            double const gap = count > 1 ? m_tiers[count - 1].first - m_tiers[count - 2].first : 1000.;
            to = std::max(to + std::max(gap, 500.), rating + 250.);
        }
        double const pad = std::max(1., to - from) * .05;
        m_low = from - pad;
        m_high = to + pad;
        buildAxis();
    }
    // 축 선, 구간 안 티어의 눈금과 점수 라벨을 그린다 (라벨이 겹치면 생략, 양 끝은 티어 이미지도 표시).
    void buildAxis() {
        m_axis->removeAllChildren();
        auto* line = CCLayerColor::create(ccc4(200, 200, 208, 150), kW - 2.f * kSide, 1.f);
        line->setPosition({kSide, kAxisY - .5f});
        m_axis->addChild(line, 0);
        std::vector<std::pair<int, std::string>> shown;
        for (auto const& tier : m_tiers)
            if (tier.first >= m_low && tier.first <= m_high) shown.push_back(tier);
        float lastLabel = -1000.f;
        float const lastX = shown.empty() ? 0.f : xAt(shown.back().first);
        for (size_t i = 0; i < shown.size(); ++i) {
            auto const& [rating, look] = shown[i];
            float const x = xAt(rating);
            auto const colour = rankedPalette(look).front();
            auto* tick = CCLayerColor::create(ccc4(colour.r, colour.g, colour.b, 235), 1.5f, 14.f);
            tick->setPosition({x - .75f, kAxisY - 6.f});
            m_axis->addChild(tick, 1);
            bool const end = i == 0 || i + 1 == shown.size();
            if (!end && (x - lastLabel < 26.f || lastX - x < 34.f)) continue;
            lastLabel = x;
            label(m_axis, std::to_string(rating), {x, kAxisY - 11.f}, .17f, 40.f, art::light(colour, .35f), false,
                  "bigFont.fnt");
            if (!end) continue;
            constexpr float kEndFace = 9.9f, kTickTop = 8.f;
            auto* face = season2Face(m_axis, look, {}, .2f);
            if (float const tall = std::max(face->getContentSize().width, face->getContentSize().height); tall > 1.f)
                face->setScale(kEndFace / tall);
            float const gap = rankedFrame(look) ? 4.f : 2.f;
            face->setPosition({x, kAxisY + kTickTop + gap + kEndFace / 2.f});
        }
        placeMarker();
    }
    // 현재 레이팅 위치에 화살표 / 아이콘 / 점수를 놓고 위아래로 흔든다. 배치 중이면 PLACEMENT 문구와 아이콘만 둔다.
    void placeMarker() {
        if (m_avatar) m_avatar->removeFromParentAndCleanup(false);
        m_marker->removeAllChildren();
        m_marker->stopAllActions();
        m_marker->setPosition({0.f, 0.f});
        if (m_rating < -1.5) return;
        if (m_rating < 0.) {
            float const placementY = kAxisY + 15.f;
            label(m_marker, "PLACEMENT", {kW / 2.f, placementY}, .3f, kW * .6f, kIce, false, "bigFont.fnt");
            showAvatar({kW / 2.f, placementY + 5.f + 1.5f + kIconHalf});
            return;
        }
        auto const colour = rankedPalette(m_look.empty() ? std::string("Unranked") : m_look).front();
        float const x = xAt(m_rating), tip = kAxisY + 3.f;
        auto* arrow = CCDrawNode::create();
        CCPoint const points[3] = {{x, tip}, {x - 5.f, tip + 8.f}, {x + 5.f, tip + 8.f}};
        arrow->drawPolygon(const_cast<CCPoint*>(points), 3, ccc4FFromccc3B(art::light(colour, .25f)), .6f,
                           ccc4f(0.f, 0.f, 0.f, .8f));
        m_marker->addChild(arrow, 1);
        float const iconY = tip + 8.f + 1.5f + kIconHalf;
        showAvatar({std::clamp(x, kIconHalf + 2.f, kW - kIconHalf - 2.f), iconY});
        auto* text = label(m_marker, fmt::format("R{:.0f}", m_rating), {x, iconY + kIconHalf + 1.5f + 3.5f}, .22f, 60.f,
                           ccWHITE, false, "bigFont.fnt");
        rankedGradient(text, m_look);
        float const half = text->getScaledContentSize().width / 2.f;
        text->setPositionX(std::clamp(x, 6.f + half, kW - 6.f - half));
        m_marker->runAction(CCRepeatForever::create(
            CCSequence::create(CCEaseSineInOut::create(CCMoveTo::create(.7f, {0.f, 2.f})),
                               CCEaseSineInOut::create(CCMoveTo::create(.7f, {0.f, 0.f})), nullptr)));
    }

public:
    static Season2Billboard* create() {
        auto* node = new Season2Billboard;
        if (node->init()) {
            node->autorelease();
            return node;
        }
        delete node;
        return nullptr;
    }
    // 티어 표를 받아 구간 설정을 하고 축을 만든다 (demon 컵은 보이는 티어 수가 더 많다).
    void setTiers(Season2Tiers const& tiers) {
        m_below = tiers.cup == "demon" ? 3 : 2;
        m_above = tiers.cup == "demon" ? 4 : 3;
        m_tiers.clear();
        for (auto const& tier : tiers.tiers) m_tiers.emplace_back(tier.minRating, tier.tier);
        std::sort(m_tiers.begin(), m_tiers.end());
        m_cup->setString((tiers.cupName.empty() ? tiers.cup : tiers.cupName).c_str());
        m_cup->limitLabelWidth(kW * .4f, .2f, .1f);
        m_index = -3;
        window();
    }
    // 내 레이팅과 티어 외형을 설정한다. 같은 값이면 아바타가 바뀐 경우에만 마커를 다시 놓는다.
    void setRating(double rating, std::string const& look) {
        if (rating == m_rating && look == m_look) {
            if (m_rating > -1.5 && avatarChanged()) placeMarker();
            return;
        }
        m_rating = rating;
        m_look = look;
        int const before = m_index;
        window();
        if (m_index == before) placeMarker();
    }
};
