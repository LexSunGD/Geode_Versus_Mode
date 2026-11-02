// 업데이트 확인 화면(UpdateCheckScene). UpdateManager의 스냅샷을 매 프레임 읽어
// 버전, 진행률, 속도, 안내 문구와 상태에 맞는 버튼(Yes/Retry/Restart/File Path)을
// 보여준다.
// 최신이면 자동으로 Versus 화면으로 넘어간다.
#include "UpdateManager.hpp"
#include "../KeyDefer.hpp"
#include "../SystemText.hpp"
#include "UpdateIntegration.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/CreatorLayer.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/utils/general.hpp>

using namespace geode::prelude;
namespace versus::update {
namespace {
// 바이트 수를 MB 문자열로 바꾼다.
std::string sizeText(uint64_t bytes) {
    return fmt::format("{:.2f} MB", bytes / (1024. * 1024.));
}
// 업데이트 확인 화면 레이어. 진행 막대(m_fill)와 상태 문구/버튼 포인터를 들고 있다.
class UpdateCheckScene final : public CCLayer {
    CCSprite* m_status = nullptr;
    CCLabelBMFont *m_version = nullptr, *m_bytes = nullptr, *m_speed = nullptr;
    CCLayerColor* m_fill = nullptr;
    CCMenuItemSpriteExtra *m_yes = nullptr, *m_no = nullptr, *m_retry = nullptr, *m_restart = nullptr,
                          *m_path = nullptr;
    bool m_leaving = false;
    float m_barWidth = 0;
    float m_restartWait = 0;
    std::string m_lastMessage;
    bool m_staffAsked = false;
    // 메뉴에 버튼을 만들어 붙인다.
    CCMenuItemSpriteExtra* button(CCMenu* menu, char const* text, SEL_MenuHandler callback, CCPoint position) {
        auto* sprite = ButtonSprite::create(text);
        sprite->setScale(.65f);
        auto* item = CCMenuItemSpriteExtra::create(sprite, this, callback);
        item->setPosition(position);
        menu->addChild(item);
        return item;
    }
    void yes(CCObject*) { UpdateManager::get().download(); }
    void retry(CCObject*) { UpdateManager::get().check(); }
    void restart(CCObject*) { UpdateManager::get().install(); }
    // 수동 설치용으로 받은 파일 경로를 클립보드에 복사하고 설치 방법을 안내한다.
    void path(CCObject*) {
        auto value = UpdateManager::get().snapshot().downloadedPath;
        if (!value.empty()) {
            utils::clipboard::write(value);
            FLAlertLayer::create(
                "Verified Download",
                ("<cg>Path copied.</c>\nIn Geode, uninstall Versus first (<cy>keep its save data</c>), "
                 "then use Install from File and select:\n" +
                 value)
                    .c_str(),
                "OK")
                ->show();
        }
    }
    void back(CCObject*) { leave(); }
    // 뒤로가기 키는 다음 틱으로 미뤄 처리한다. 설치/재시작 중에는 나갈 수 없다.
    void keyBackClicked() override {
        versus::deferBack(this, [this] { leave(); });
    }
    // 화면을 나간다. 진행 중인 다운로드는 일시정지한다.
    void leave() {
        auto state = UpdateManager::get().snapshot().state;
        if (m_leaving || state == UpdateState::Installing || state == UpdateState::Restarting) return;
        m_leaving = true;
        UpdateManager::get().pause();
        CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(.2f, CreatorLayer::scene()));
    }
    // 매 프레임 상태를 화면에 반영한다. 기록 전송을 기다리는 중이면 설치를 다시 시도하고,
    // 최신 상태이면 씬 전환이 끝난 뒤 Versus 화면으로 이동한다.
    void update(float dt) override {
        if (m_leaving) return;
        if (UpdateManager::get().waitingForRecords()) UpdateManager::get().install();
        auto s = UpdateManager::get().snapshot();
#if defined(GEODE_IS_ANDROID) || defined(GEODE_IS_IOS)
        // Android/iOS에서 재시작 후 30초가 지나도 앱이 다시 열리지 않으면 재시작 대기 상태를 풀어
        // 준다.
        if (s.state == UpdateState::Restarting) {
            m_restartWait += std::max(0.f, dt);
            if (m_restartWait > 30.f) {
                UpdateManager::get().restartTimeout();
                m_restartWait = 0;
            }
        } else
            m_restartWait = 0;
#endif
        if (s.state == UpdateState::Current) {
            auto* director = CCDirector::sharedDirector();
            if (director->getRunningScene() != getParent() || director->getNextScene()) return;
            m_leaving = true;
            enterVersus();
            return;
        }
        // 서버 버전보다 설치된 버전이 높아 오류가 난 경우, 운영진 계정이면 한 번 다시 확인한다.
        if (s.state == UpdateState::Error && s.newerThanServer && !s.busy && !m_staffAsked) {
            m_staffAsked = true;
            checkStaffAccount([self = Ref<UpdateCheckScene>(this)](bool manager) {
                if (manager && !self->m_leaving && self->getParent()) UpdateManager::get().check();
            });
        }
        if (s.message != m_lastMessage) {
            m_lastMessage = s.message;
            versus::setSystemText(m_status, s.message);
        }
        m_version->setString(fmt::format("v{}  >  {}{}", s.installedVersion, s.manifest.version.empty() ? "" : "v",
                                         s.manifest.version.empty() ? "..." : s.manifest.version)
                                 .c_str());
        // 진행률: 패치 전송량이 있으면 그것을, 없으면 전체 다운로드량을 기준으로 계산한다.
        uint64_t const total = s.transferTotal ? s.transferTotal : s.manifest.size;
        uint64_t const done = s.transferTotal ? std::min(s.transferDone, total) : s.downloaded;
        float fraction = total ? std::clamp(float(done) / float(total), 0.f, 1.f) : 0.f;
        m_fill->setScaleX(fraction);
        m_bytes->setString(fmt::format("{:.0f}%   {} / {}", fraction * 100.f, sizeText(done), sizeText(total)).c_str());
        std::string speed = s.bytesPerSecond > 0 ? fmt::format("{:.0f} KB/s    ETA {:.0f}s", s.bytesPerSecond / 1024.,
                                                               (total - std::min(done, total)) / s.bytesPerSecond)
                                                 : "";
        if (s.state == UpdateState::UpdateAvailable) speed = "Download update?  Yes / No";
        if (s.state == UpdateState::ReadyToInstall) speed = "Geometry Dash will close, update, and restart.";
        if (s.state == UpdateState::VerifiedDownloaded) speed = "Manual installation only. File path available below.";
        m_speed->setString(speed.c_str());
        m_speed->limitLabelWidth(m_barWidth, .42f, .24f);
        m_yes->setVisible(s.state == UpdateState::UpdateAvailable && !s.busy);
        m_retry->setVisible(s.state == UpdateState::Error && !s.busy);
        m_restart->setVisible(s.state == UpdateState::ReadyToInstall && !s.busy);
        m_path->setVisible(s.state == UpdateState::VerifiedDownloaded && !s.busy);
        m_no->setVisible(s.state != UpdateState::Installing && s.state != UpdateState::Restarting);
    }

public:
    // UI 구성. 제목, 버전 문구, 상태 문구, 진행 막대,
    // 상태별 버튼을 만들고 업데이트 확인을 시작한다.
    bool init() override {
        if (!CCLayer::init()) return false;
        setKeypadEnabled(true);
        auto size = CCDirector::sharedDirector()->getWinSize();
        addChild(CCLayerColor::create(ccc4(15, 21, 33, 255), size.width, size.height));
        auto* title = CCLabelBMFont::create("VERSUS UPDATE", "bigFont.fnt");
        title->setScale(.8f);
        title->setPosition({size.width / 2, size.height - 42});
        addChild(title);
        m_version = CCLabelBMFont::create("Checking...", "bigFont.fnt");
        m_version->setScale(.46f);
        m_version->setPosition({size.width / 2, size.height * .68f});
        addChild(m_version);
        m_status =
            versus::createSystemText("Checking server version...", 14.f, size.width - 64, kCCTextAlignmentCenter);
        m_status->setPosition({size.width / 2, size.height * .48f});
        addChild(m_status);
        m_barWidth = size.width - 80;
        auto* track = CCLayerColor::create(ccc4(54, 63, 79, 255), m_barWidth, 14);
        track->setPosition({40, 99});
        addChild(track);
        m_fill = CCLayerColor::create(ccc4(78, 196, 255, 255), m_barWidth, 14);
        m_fill->setAnchorPoint({0, 0});
        m_fill->setPosition({40, 99});
        m_fill->setScaleX(0);
        addChild(m_fill);
        m_bytes = CCLabelBMFont::create("", "bigFont.fnt");
        m_bytes->setScale(.4f);
        m_bytes->setPosition({size.width / 2, 128});
        addChild(m_bytes);
        m_speed = CCLabelBMFont::create("", "chatFont.fnt");
        m_speed->setScale(.42f);
        m_speed->setPosition({size.width / 2, 80});
        addChild(m_speed);
        auto* menu = CCMenu::create();
        menu->setPosition({0, 0});
        addChild(menu);
        CCPoint action{size.width * .64f, 39}, cancel{size.width * .33f, 39};
        m_yes = button(menu, "Yes", menu_selector(UpdateCheckScene::yes), action);
        m_no = button(menu, "No / Back", menu_selector(UpdateCheckScene::back), cancel);
        m_retry = button(menu, "Retry", menu_selector(UpdateCheckScene::retry), action);
        auto* restartSprite = CCSprite::createWithSpriteFrameName("GJ_updateBtn_001.png");
        restartSprite->setScale(.75f);
        m_restart = CCMenuItemSpriteExtra::create(restartSprite, this, menu_selector(UpdateCheckScene::restart));
        m_restart->setPosition(action);
        menu->addChild(m_restart);
        m_path = button(menu, "File Path", menu_selector(UpdateCheckScene::path), action);
        UpdateManager::get().check();
        scheduleUpdate();
        update(0);
        return true;
    }
    CREATE_FUNC(UpdateCheckScene);
};
}
// 업데이트 확인 화면을 연다. 대전 중이면 확인 없이 Versus 화면으로 간다.
void showUpdateCheck() {
    if (matchInProgress()) {
        enterVersus();
        return;
    }
    auto* scene = CCScene::create();
    auto* layer = UpdateCheckScene::create();
    if (!layer) return;
    scene->addChild(layer);
    CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(.2f, scene));
}
}
