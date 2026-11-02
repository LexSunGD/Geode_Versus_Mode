// 디버그 모드 헬퍼의 헤더. 버전 문자열 파싱/비교, 제작자·디버그·스태프 계정 판별,
// 디버그 모드 상태 API 선언을 모아 둔다.
// 순수 함수는 여기서 inline 으로 구현하고, 저장값을 읽는 상태 함수는 DebugMode.cpp 에 있다.
#pragma once
#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace versus::debug {

// "v1.2.3" 또는 "1.2.3" 형식을 {major, minor, patch} 로 파싱한다.
// 각 숫자는 최대 6자리이며, 형식이 틀리거나 뒤에 남는 글자가 있으면 nullopt.
inline std::optional<std::array<int, 3>> parseVersion(std::string_view text) {
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) text.remove_prefix(1);
    std::array<int, 3> parts{};
    for (int i = 0; i < 3; ++i) {
        size_t digits = 0;
        int value = 0;
        while (digits < text.size() && text[digits] >= '0' && text[digits] <= '9' && digits < 6) {
            value = value * 10 + (text[digits] - '0');
            ++digits;
        }
        if (digits == 0) return std::nullopt;
        parts[i] = value;
        text.remove_prefix(digits);
        if (i < 2) {
            if (text.empty() || text.front() != '.') return std::nullopt;
            text.remove_prefix(1);
        }
    }
    if (!text.empty()) return std::nullopt;
    return parts;
}
// 클라이언트 빌드 버전이 서버 버전보다 높으면 true. 둘 중 하나라도 파싱 실패면 false.
inline bool newerThan(std::string_view build, std::string_view server) {
    auto const a = parseVersion(build), b = parseVersion(server);
    return a && b && *a > *b;
}
// 제작자 계정인지 판별한다. 이름은 대소문자를 무시하고 비교하며, 유효한 accountId(>0)가 있어야 한다.
inline bool creatorName(std::string_view name, int accountId) {
    constexpr std::string_view creator = "tipp7";
    if (accountId <= 0 || name.size() != creator.size()) return false;
    for (size_t i = 0; i < name.size(); ++i) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c != creator[i]) return false;
    }
    return true;
}
// 제작자 외에 디버그 기능을 허용하는 테스트용 계정의 GD accountId
inline constexpr int kTestAccountId = 43728829;
// 제작자 계정이거나 테스트 계정이면 디버그 계정으로 본다.
inline bool debugAccount(std::string_view name, int accountId) {
    return creatorName(name, accountId) || accountId == kTestAccountId;
}
// 서버로 보낼 버전 문자열을 항상 소문자 "v" 접두사가 붙은 형태로 맞춘다.
inline std::string wireForm(std::string_view version) {
    if (!version.empty() && (version.front() == 'v' || version.front() == 'V'))
        return "v" + std::string(version.substr(1));
    return "v" + std::string(version);
}

// 스태프(매니저) 계정 ID 목록을 쉼표로 이어 저장하는 Mod 저장값 키와, 목록 최대 개수
inline constexpr char const* kStaffAccountsKey = "debug-staff-accounts";
inline constexpr size_t kStaffAccountsMax = 16;
// "1,2,3" 형식의 문자열을 accountId 목록으로 변환한다.
// 9자리 초과, 숫자가 아닌 항목, 0 이하, 중복은 버린다.
inline std::vector<int> listedAccounts(std::string_view list) {
    std::vector<int> result;
    while (!list.empty()) {
        auto const comma = list.find(',');
        auto const item = list.substr(0, comma);
        int value = 0;
        bool valid = !item.empty() && item.size() <= 9;
        for (char c : item) {
            if (c < '0' || c > '9') {
                valid = false;
                break;
            }
            value = value * 10 + (c - '0');
        }
        if (valid && value > 0 && std::find(result.begin(), result.end(), value) == result.end())
            result.push_back(value);
        if (comma == std::string_view::npos) break;
        list.remove_prefix(comma + 1);
    }
    return result;
}
// 목록 문자열에 accountId 가 들어 있는지 확인한다.
inline bool listedAccount(std::string_view list, int accountId) {
    if (accountId <= 0) return false;
    auto const ids = listedAccounts(list);
    return std::find(ids.begin(), ids.end(), accountId) != ids.end();
}
// 목록에서 accountId 를 갱신한 새 문자열을 만든다.
// manager 이면 맨 앞에 넣고, 아니면 제거하며, 최대 kStaffAccountsMax 개로 자른다.
inline std::string withAccount(std::string_view list, int accountId, bool manager) {
    auto ids = listedAccounts(list);
    ids.erase(std::remove(ids.begin(), ids.end(), accountId), ids.end());
    if (manager && accountId > 0) ids.insert(ids.begin(), accountId);
    if (ids.size() > kStaffAccountsMax) ids.resize(kStaffAccountsMax);
    std::string text;
    for (int id : ids) text += (text.empty() ? "" : ",") + std::to_string(id);
    return text;
}

// 아래는 DebugMode.cpp 에서 구현. 현재 로그인한 GD 계정을 기준으로 판단한다.
// creatorAccount: 제작자 계정 / debugAccountNow: 디버그 계정 또는 스태프 목록 계정
// staffAccountNow: 이 기기에 저장된 스태프 목록에 포함된 계정
bool creatorAccount();
bool debugAccountNow();
bool staffAccountNow();
// 서버가 알려 준 스태프 여부를 이 기기의 저장 목록에 반영한다.
void noteStaff(int accountId, bool manager);
// 빌드가 서버보다 새 버전일 때 디버그 모드를 켠다. 서버 버전 표기로 접속하게 만든다.
bool activate(std::string const& serverVersion);
// 서버 버전과 최신 버전 알림을 반영한다. 서버가 빌드 버전을 따라잡으면 디버그 모드가 끝난다.
void noteServer(std::string const& serverVersion);
// 디버그 모드가 켜져 있고 디버그 계정이면 true
bool active();
// 디버그 모드 스위치(사용자 설정) 상태와 변경
bool enabled();
void setEnabled(bool on);
void noteLatest(std::string const& latest);
// 서버에 보낼 버전 문자열과 화면에 띄울 디버그 배지 문구
std::string const& wireVersion();
std::string badgeText();

// 요청에 사용할 API 루트 경로
char const* apiRoot();
}
