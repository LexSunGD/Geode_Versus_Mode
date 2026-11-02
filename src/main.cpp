// 모드 진입점. 모드 로드 시 에디션 정보를 로그로 남기고 업데이트 부팅 확인을 하며,
// CreatorLayer 의 기본 Versus 버튼을 이 모드의 화면으로 연결한다.
#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "Edition.hpp"
#include "update/UpdateManager.hpp"

using namespace geode::prelude;

// 모드가 로드되면 에디션 표시를 기록하고, 자체 업데이트 후 첫 부팅임을 확인 처리한다.
$on_mod(Loaded) {
    log::info("{}", versus::editionMarker());
    versus::update::confirmUpdateBoot();
#if VERSUS_BOOTSTRAP
    log::info("versus-bootstrap:1");
#endif
}

// 메인 메뉴가 뜨면 업데이트 부팅 확인을 한 번 더 한다(모드 로드 시점에 놓친 경우 대비).
class $modify(VersusUpdateBootReceipt, MenuLayer){bool init(){if (!MenuLayer::init()) return false;
versus::update::confirmUpdateBoot();
return true;
}
}
;

// CreatorLayer 의 내장 Versus 버튼을 활성화하고, 눌렀을 때 이 모드의 진입 흐름이 열리게 한다.
class $modify(VersusCreatorLayer, CreatorLayer){bool init(){if (!CreatorLayer::init()) return false;

// 기본 버튼을 ID 로 찾는다. 없으면 경고만 남기고 그대로 둔다.
auto* menu = this->getChildByID("creator-buttons-menu");
auto* button = menu ? typeinfo_cast<CCMenuItemSpriteExtra*>(menu->getChildByID("versus-button")) : nullptr;
if (!button) {
    log::warn("Unable to find the built-in Versus button");
    return true;
}

// 활성 상태 스프라이트로 교체하되 기존 스프라이트의 크기를 유지한다.
auto* activeSprite = CCSprite::createWithSpriteFrameName("GJ_versusBtn_001.png");
if (activeSprite) {
    if (auto* oldSprite = button->getChildByType<CCSprite*>(0)) {
        activeSprite->setScale(oldSprite->getScale());
    }
    button->setSprite(activeSprite);
}
button->setEnabled(true);
button->setTarget(this, menu_selector(VersusCreatorLayer::onOpenVersus));

// 버튼 위에 "Versus Mode!" 라벨을 붙인다.
auto* title = CCLabelBMFont::create("Versus Mode!", "goldFont.fnt");
title->setScale(.38f);
title->limitLabelWidth(button->getContentSize().width + 38.f, .38f, .25f);
title->setPosition({button->getPositionX(), button->getPositionY() + button->getContentSize().height / 2.f + 10.f});
title->setID("versus-mode-label"_spr);
menu->addChild(title, 10);

return true;
}

// 버튼 클릭 시 업데이트 확인부터 진행한다.
void onOpenVersus(CCObject*) {
    versus::update::showUpdateCheck();
}
}
;
