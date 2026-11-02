// 에디터 일시정지 메뉴의 "Plaza Test" 버튼 구현 (디버그 모드 전용).
// 편집 중인 플랫포머 레벨을 저장하고 나간 뒤, 그 레벨을 Season 2 plaza 맵으로 쓰며
// Versus를 연다. Probe 노드가 매 프레임 현재 씬을 훑으며 단계(Phase)를 진행시킨다.
#include "EditorPlaza.hpp"
#include "DebugMode.hpp"
#include "Season2Lounge.hpp"
#include "WeakRefs.hpp"
#include "update/UpdateManager.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/EditLevelLayer.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>

#include <algorithm>
#include <string>

using namespace geode::prelude;

namespace versus::editorplaza {
namespace {
// Plaza Test 진행 단계.
// Idle: 대기, Leaving: 저장 후 레벨 페이지로 나가는 중,
// Entry: Versus가 Season 2에 들어오길 기다리는 중, Running: plaza 실행 중.
enum class Phase {
    Idle,
    Leaving,
    Entry,
    Running,
};
// 씬 검사 주기(초)와, Leaving / Entry 단계의 제한 시간(초).
constexpr float kScanEvery = .2f;
constexpr float kLeaveLimit = 30.f;
constexpr float kEntryLimit = 45.f;
constexpr char const* kButtonID = "plaza-test-button"_spr;

// Plaza Test 전체 상태. level은 테스트 대상 레벨, timer는 현재 단계의 경과 시간(초).
struct State {
    Phase phase = Phase::Idle;
    Ref<GJGameLevel> level;
    float timer = 0.f;
};
State& state() {
    static auto* value = new State;
    return *value;
}

// Plaza Test를 끝내고 상태와 에디터 맵 지정을 초기화한다. 이미 Idle이면 아무것도 안 함.
void finish(char const* why) {
    auto& s = state();
    if (s.phase == Phase::Idle) return;
    log::info("Versus Plaza Test: {}", why);
    s.phase = Phase::Idle;
    s.level = nullptr;
    s.timer = 0.f;
    lounge::clearEditorMap();
}

// 씬(또는 그 안의 LevelEditorLayer)에서 EditorPauseLayer를 찾는다. 없으면 nullptr.
EditorPauseLayer* pauseIn(CCScene* scene) {
    if (auto* pause = scene->getChildByType<EditorPauseLayer>(0)) return pause;
    if (auto* editor = scene->getChildByType<LevelEditorLayer>(0)) return editor->getChildByType<EditorPauseLayer>(0);
    return nullptr;
}

// node에서 부모 방향으로 올라가며 처음 만나는 T 타입 노드를 찾는다.
template <class T> T* ancestor(CCNode* node) {
    for (; node; node = node->getParent())
        if (auto* found = typeinfo_cast<T*>(node)) return found;
    return nullptr;
}

// 씬을 주기적으로 검사하는 보이지 않는 노드. 일시정지 메뉴에 버튼을 붙이고,
// 레벨 페이지 도착을 감지하며, 제한 시간 초과 시 테스트를 중단한다.
class Probe final : public CCNode {
    float m_scan = 0.f;

    // 일시정지 메뉴에 "Plaza Test" 버튼을 한 번만 추가한다.
    // GD의 save-and-exit 버튼 크기를 따라 만들고, 메뉴에 레이아웃이 있으면 거기에 넣고,
    // 없으면 자체 메뉴를 만들어 그 버튼 아래(공간이 없으면 화면 왼쪽 아래)에 둔다.
    void decorate(EditorPauseLayer* pause) {
        if (pause->getChildByIDRecursive(kButtonID)) return;
        auto* menu = typeinfo_cast<CCMenu*>(pause->getChildByID("resume-menu"));
        auto* like = menu ? typeinfo_cast<CCMenuItemSpriteExtra*>(menu->getChildByID("save-and-exit-button")) : nullptr;
        auto* likeSprite = like ? typeinfo_cast<ButtonSprite*>(like->getNormalImage()) : nullptr;
        ButtonSprite* sprite = nullptr;
        if (likeSprite) {
            auto const size = likeSprite->getContentSize();
            sprite = ButtonSprite::create("Plaza Test", static_cast<int>(size.width), true, "goldFont.fnt",
                                          "GJ_button_01.png", size.height, .8f);
            if (sprite) sprite->setScale(likeSprite->getScale());
        }
        if (!sprite) sprite = ButtonSprite::create("Plaza Test", "goldFont.fnt", "GJ_button_01.png", .8f);
        if (!sprite) return;
        auto* button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(Probe::onPlaza));
        if (!button) return;
        button->setID(kButtonID);
        if (menu && menu->getLayout()) {
            menu->addChild(button);
            menu->updateLayout();
            return;
        }
        auto* own = CCMenu::create();
        own->setID("plaza-test-menu"_spr);
        own->setPosition({0.f, 0.f});
        pause->addChild(own, 10);
        auto const window = CCDirector::sharedDirector()->getWinSize();
        auto const half = button->getScaledContentSize() / 2.f;
        CCPoint at{8.f + half.width, 8.f + half.height};
        if (like && like->getParent()) {
            float lowest = like->getPositionY();
            for (auto* item : CCArrayExt<CCNode*>(like->getParent()->getChildren()))
                lowest = std::min(lowest, item->getPositionY());
            auto const world = like->getParent()->convertToWorldSpace({like->getPositionX(), lowest});
            float const y = world.y - like->getScaledContentSize().height / 2.f - 8.f - half.height;
            if (y - half.height >= 4.f) at = CCPoint{world.x, y};
        }
        at.x = std::clamp(at.x, half.width + 4.f, window.width - half.width - 4.f);
        button->setPosition(own->convertToNodeSpace(at));
        own->addChild(button);
    }

    // Save and Exit 후 대상 레벨의 EditLevelLayer가 열렸을 때 호출됨.
    // 저장된 레벨 문자열을 에디터 맵으로 등록하고(플랫포머가 아니면 중단), Entry 단계로 넘어가 Versus를 연다.
    void arrived(EditLevelLayer* page) {
        auto& s = state();
        if (!s.level || page->m_level != s.level.data()) return;
        if (!lounge::setEditorMap(std::string(s.level->m_levelString))) {
            finish("the saved level is not a platformer level");
            FLAlertLayer::create("Plaza Test", "The plaza needs a <cy>platformer</c> level.", "OK")->show();
            return;
        }
        s.phase = Phase::Entry;
        s.timer = 0.f;
        log::info("Versus Plaza Test: \"{}\" is both cups' plaza map; opening Versus",
                  std::string(s.level->m_levelName));
        update::showUpdateCheck();
    }

    // Leaving 단계 시작: 대상 레벨을 기억하고 GD의 Save and Exit를 실행해 저장 후 에디터를 나간다.
    void start(EditorPauseLayer* pause) {
        auto* editor = pause->m_editorLayer;
        if (!editor || !editor->m_level || state().phase != Phase::Idle) return;
        auto& s = state();
        s.phase = Phase::Leaving;
        s.level = editor->m_level;
        s.timer = 0.f;
        log::info("Versus Plaza Test: saving \"{}\" and leaving the editor (GD's Save and Exit)",
                  std::string(editor->m_level->m_levelName));
        if (auto* save = typeinfo_cast<CCMenuItemSpriteExtra*>(pause->getChildByIDRecursive("save-and-exit-button")))
            save->activate();
        else
            pause->onSaveAndExit(nullptr);
    }

public:
    static Probe* create() {
        auto* probe = new Probe;
        if (probe->init()) {
            probe->autorelease();
            return probe;
        }
        delete probe;
        return nullptr;
    }
    // 버튼 콜백. 디버그 모드이고 플랫포머 레벨일 때만 확인 팝업을 띄우고,
    // 확인하면 다음 메인 스레드 틱에서 start()를 실행한다 (팝업이 닫힌 뒤에 진행하기 위함).
    void onPlaza(CCObject* sender) {
        auto* pause = ancestor<EditorPauseLayer>(typeinfo_cast<CCNode*>(sender));
        if (!pause || !pause->m_editorLayer || !pause->m_editorLayer->m_level || !debug::active()) return;
        if (state().phase != Phase::Idle) return;
        auto* settings = pause->m_editorLayer->m_levelSettings;
        if (!settings || !settings->m_platformerMode) {
            FLAlertLayer::create("Plaza Test",
                                 "The plaza needs a <cy>platformer</c> level "
                                 "(the level's settings: Platformer).",
                                 "OK")
                ->show();
            return;
        }
        createQuickPopup("Plaza Test",
                         "<cy>Save</c> this level and enter <cg>Season 2</c>'s plaza with it as the map?\n"
                         "Both cups use it until you come back here.",
                         "Cancel", "Enter", [weak = Weak<EditorPauseLayer>(pause)](auto*, bool enter) {
                             if (!enter) return;
                             Loader::get()->queueInMainThread([weak] {
                                 auto pause = weak.lock();
                                 if (!pause || !pause->getParent() || !debug::active() || !s_probe) return;
                                 s_probe->start(pause.data());
                             });
                         });
    }
    // Leaving / Entry 단계는 제한 시간을 넘기면 중단한다.
    // 그 외에는 kScanEvery마다 씬을 검사한다. 씬 전환 중에는 건너뛴다.
    // Leaving이면 EditLevelLayer 도착을 확인하고, Idle이면 일시정지 메뉴에 버튼을 붙인다.
    void update(float dt) override {
        auto& s = state();
        dt = std::clamp(dt, 0.f, .25f);
        if (s.phase == Phase::Leaving || s.phase == Phase::Entry) {
            s.timer += dt;
            if (s.phase == Phase::Leaving && s.timer > kLeaveLimit)
                finish("GD's level page did not come");
            else if (s.phase == Phase::Entry && s.timer > kEntryLimit)
                finish("Versus did not reach Season 2 in time");
        }
        if ((m_scan += dt) < kScanEvery) return;
        m_scan = 0.f;
        if (s.phase != Phase::Leaving && !debug::active()) return;
        auto* director = CCDirector::sharedDirector();
        auto* scene = director->getRunningScene();
        if (!scene || director->getNextScene() || typeinfo_cast<CCTransitionScene*>(scene)) return;
        if (s.phase == Phase::Leaving) {
            if (auto* page = scene->getChildByType<EditLevelLayer>(0)) arrived(page);
            return;
        }
        if (s.phase == Phase::Idle)
            if (auto* pause = pauseIn(scene)) decorate(pause);
    }
    static inline Probe* s_probe = nullptr;
};
}

// Versus 쪽에서 쓰는 상태 조회 / 전환 함수.
// waiting: Entry 중인지, entered: Season 2 진입 시 Running으로 전환,
// homeReached: Versus 메인 화면에 도달하면 종료, cancel: 중도 취소,
// backToLevel: 실행 중이면 테스트를 끝내고 대상 레벨의 페이지로 돌아감.
bool waiting() {
    return state().phase == Phase::Entry;
}
void entered() {
    auto& s = state();
    if (s.phase != Phase::Entry) return;
    s.phase = Phase::Running;
    s.timer = 0.f;
}
void homeReached() {
    if (state().phase == Phase::Running) finish("over (Versus' main screen)");
}
void cancel() {
    finish("dropped");
}
bool backToLevel() {
    auto& s = state();
    if (s.phase != Phase::Running || !s.level) return false;
    Ref<GJGameLevel> const level = s.level;
    finish("over (back to the level's page)");
    CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(.5f, EditLevelLayer::scene(level.data())));
    return true;
}
}

// 모드 로드 후 Probe를 한 번 만들어 유지하고 스케줄러에 등록한다.
$on_mod(Loaded) {
    Loader::get()->queueInMainThread([] {
        using versus::editorplaza::Probe;
        if (Probe::s_probe) return;
        Probe::s_probe = Probe::create();
        if (!Probe::s_probe) return;
        Probe::s_probe->retain();
        CCDirector::sharedDirector()->getScheduler()->scheduleUpdateForTarget(Probe::s_probe, 0, false);
    });
}
