// bootstrap(최초 설치기) 빌드용 UpdateIntegration 구현. 설치기에는 Service가 없으므로
// 대전/기록은 없는 것으로 처리하고, Firebase 익명 로그인으로 업데이트 인증 토큰을 얻는다.
#include "../src/update/UpdateIntegration.hpp"
#include "../src/FirebaseConfig.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/binding/CreatorLayer.hpp>
using namespace geode::prelude;
namespace versus::update {
// 설치기에는 대전과 미전송 기록이 없다.
bool matchInProgress() {
    return false;
}
bool recordsPending() {
    return false;
}
namespace {
// URL 쿼리용 퍼센트 인코딩(영숫자와 -_.~ 외에는 %XX).
std::string encode(std::string const& input) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : input) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~')
            result += c;
        else {
            result += '%';
            result += hex[c >> 4];
            result += hex[c & 15];
        }
    }
    return result;
}
}
// 저장된 refresh token이 있으면 토큰을 갱신하고, 없으면 익명 계정을 새로 만든다.
// 응답에서 토큰, refresh token, uid를 받아 저장하고 callback(토큰, 오류 문구)를 호출한다.
void authorizeUpdate(std::function<void(std::string, std::string)> callback) {
    auto* mod = Mod::get();
    auto refresh = mod->getSavedValue<std::string>("firebase-refresh-token", "");
    auto key = mod->getSettingValue<std::string>("firebase-api-key");
    if (key.empty()) key = firebase_config::WEB_API_KEY;
    auto req = web::WebRequest();
    req.timeout(std::chrono::seconds(20));
    req.certVerification(true);
    req.followRedirects(false);
    req.param("key", key);
    bool refreshing = !refresh.empty();
    std::string url;
    if (refreshing) {
        url = "https://securetoken.googleapis.com/v1/token";
        req.header("Content-Type", "application/x-www-form-urlencoded");
        req.bodyString("grant_type=refresh_token&refresh_token=" + encode(refresh));
    } else {
        url = "https://identitytoolkit.googleapis.com/v1/accounts:signUp";
        req.header("Content-Type", "application/json");
        req.bodyString("{\"returnSecureToken\":true}");
    }
    async::spawn(req.post(url), [callback = std::move(callback), refreshing](web::WebResponse response) {
        auto parsed = response.json();
        if (!response.ok() || !parsed || !parsed.unwrap().isObject()) {
            callback({}, "Sign-in unavailable. Retry; existing account data is unchanged.");
            return;
        }
        auto body = parsed.unwrap();
        auto field = [&](char const* name) {
            return body.get(name).andThen([](matjson::Value const& v) { return v.asString(); }).unwrapOr("");
        };
        auto token = field(refreshing ? "id_token" : "idToken");
        auto refreshToken = field(refreshing ? "refresh_token" : "refreshToken");
        auto uid = field(refreshing ? "user_id" : "localId");
        if (token.empty() || refreshToken.empty() || uid.empty()) {
            callback({}, "Invalid sign-in response");
            return;
        }
        auto* mod = Mod::get();
        mod->setSavedValue("firebase-refresh-token", refreshToken);
        mod->setSavedValue("firebase-uid", uid);
        if (mod->saveData().isErr()) {
            callback({}, "Cannot save account identity. Check storage and retry.");
            return;
        }
        callback(std::move(token), {});
    });
}
// 설치기에서는 Versus 화면 대신 Creator 화면으로 이동한다.
void enterVersus() {
    CCDirector::sharedDirector()->replaceScene(CreatorLayer::scene());
}
}
