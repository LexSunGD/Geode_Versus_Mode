// 디버그 모드 상태 구현. 빌드 버전이 서버보다 앞설 때 제작자/스태프 계정에게만
// 서버 버전으로 위장해 접속하게 하고, API 루트 선택을 담당한다.
#include "DebugMode.hpp"
#include "FirebaseConfig.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJAccountManager.hpp>

using namespace geode::prelude;

namespace versus::debug {
namespace {
// 디버그 모드의 런타임 상태: 활성 여부, 안내 팝업 표시 여부, 서버 버전,
// 서버로 보낼 버전(wire), 사용자 스위치 캐시
struct DebugState {
    bool active = false;
    bool told = false;
    std::string server;
    std::string wire = firebase_config::CLIENT_VERSION;
    std::optional<bool> enabled;
};
// 종료 시 소멸자 순서 문제를 피하려고 의도적으로 해제하지 않는 싱글턴
DebugState& debugState() {
    static auto* value = new DebugState;
    return *value;
}
// 디버그 모드 스위치를 저장하는 Mod 저장값 키
constexpr char const* kSwitchKey = "debug-mode";
}

// 현재 로그인한 GD 계정이 제작자 계정인지 확인
bool creatorAccount() {
    auto* account = GJAccountManager::sharedState();
    return account && creatorName(std::string(account->m_username), account->m_accountID);
}

// 디버그 계정(제작자/테스트 계정) 또는 저장된 스태프 목록에 있는 계정이면 true
bool debugAccountNow() {
    auto* account = GJAccountManager::sharedState();
    return account && (debugAccount(std::string(account->m_username), account->m_accountID) || staffAccountNow());
}

// 이 기기에 저장된 스태프 목록에 현재 계정이 있는지 확인
bool staffAccountNow() {
    auto* account = GJAccountManager::sharedState();
    auto* mod = Mod::get();
    return account && mod && account->m_accountID > 0 &&
           listedAccount(mod->getSavedValue<std::string>(kStaffAccountsKey, ""), account->m_accountID);
}

// 스태프 목록에 계정을 추가/제거해 저장한다. 변화가 없으면 아무것도 하지 않는다.
void noteStaff(int accountId, bool manager) {
    auto* mod = Mod::get();
    if (!mod || accountId <= 0) return;
    auto const current = mod->getSavedValue<std::string>(kStaffAccountsKey, "");
    auto const next = withAccount(current, accountId, manager);
    if (next == current || listedAccount(current, accountId) == manager) return;
    mod->setSavedValue<std::string>(kStaffAccountsKey, next);
    log::info("Versus: GD account {} {} this device's managers (debug builds)", accountId,
              manager ? "joins" : "leaves");
}

// 디버그 계정이고 빌드가 서버보다 새 버전이면 디버그 모드를 활성화하고 true 반환.
// 서버가 이해하는 버전(서버 버전)으로 접속하도록 wire 버전을 서버 값으로 바꾼다.
// 스위치가 켜져 있으면 안내 팝업을 세션당 한 번만 띄운다.
bool activate(std::string const& serverVersion) {
    auto& s = debugState();
    if (!debugAccountNow() || !newerThan(firebase_config::CLIENT_VERSION, serverVersion)) return false;
    s.active = true;
    s.server = wireForm(serverVersion);
    s.wire = s.server;
    if (!enabled()) {
        log::info("Versus: build {} connects as {} (debug mode switched off)", firebase_config::CLIENT_VERSION,
                  s.server);
        return true;
    }
    if (!std::exchange(s.told, true)) {
        log::warn("Versus DEBUG MODE: build {} on server {} (creator account)", firebase_config::CLIENT_VERSION,
                  s.server);
        auto const body =
            fmt::format("This build (<cg>{}</c>) is newer than the server (<cy>{}</c>).\n"
                        "Debug mode is on for your account only: Versus connects as {}.\n"
                        "The Ranked screen has a <cy>BOT</c> button for a local battle against a bot (best 10%).\n"
                        "Server features added in this build work after the server update.",
                        firebase_config::CLIENT_VERSION, s.server, s.server);
        Loader::get()->queueInMainThread(
            [body] { FLAlertLayer::create(nullptr, "Debug Mode", body, "OK", nullptr, 380.f)->show(); });
    }
    return true;
}

// 서버 버전 문자열이 올바르면 기록해 둔다.
void noteServer(std::string const& serverVersion) {
    if (parseVersion(serverVersion)) debugState().server = wireForm(serverVersion);
}

// 디버그 모드 스위치 값. 저장값이 없으면 제작자 계정이거나 활성 상태일 때 켜진 것으로 본다.
bool enabled() {
    auto& s = debugState();
    if (!s.enabled) {
        auto* mod = Mod::get();
        if (!mod || !mod->hasSavedValue(kSwitchKey)) return creatorAccount() || s.active;
        s.enabled = mod->getSavedValue<bool>(kSwitchKey, true);
    }
    return *s.enabled;
}

// 스위치를 바꿔 저장한다. 디버그 계정이 아니면 무시.
void setEnabled(bool on) {
    if (!debugAccountNow()) return;
    debugState().enabled = on;
    if (auto* mod = Mod::get()) mod->setSavedValue<bool>(kSwitchKey, on);
    log::info("Versus debug mode switched {}", on ? "on" : "off");
}

bool active() {
    return enabled() && debugAccountNow();
}

// 서버가 알린 최신 버전이 빌드 버전과 같아지면 디버그 모드를 끝내고
// 실제 빌드 버전으로 접속하도록 되돌린다.
void noteLatest(std::string const& latest) {
    auto& s = debugState();
    if (!s.active) return;
    auto const server = parseVersion(latest), build = parseVersion(firebase_config::CLIENT_VERSION);
    if (!server || !build || *server != *build) return;
    s.active = false;
    s.server = wireForm(latest);
    s.wire = firebase_config::CLIENT_VERSION;
    log::info("Versus debug mode ended: the server now runs {}", s.server);
}

// 서버로 보낼 클라이언트 버전 문자열
std::string const& wireVersion() {
    return debugState().wire;
}

// 화면에 표시할 디버그 배지 문구. 디버그 모드가 꺼져 있으면 빈 문자열.
std::string badgeText() {
    auto const& s = debugState();
    if (!active()) return {};
    if (s.server.empty()) return fmt::format("DEBUG {}", firebase_config::CLIENT_VERSION);
    return fmt::format("DEBUG {} (server {})", firebase_config::CLIENT_VERSION, s.server);
}

// 요청에 사용할 API 루트 경로
char const* apiRoot() {
    return firebase_config::ROOT;
}
}
