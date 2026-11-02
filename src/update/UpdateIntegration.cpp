// UpdateIntegration.hpp의 구현: 각 함수가 Service와 UI 함수를 그대로 호출한다.
#include "UpdateIntegration.hpp"
#include "../VersusService.hpp"
#include "../VersusUI.hpp"
namespace versus::update {
bool matchInProgress() {
    return Service::get().battleInProgress();
}
bool recordsPending() {
    return Service::get().recordsPending();
}
void authorizeUpdate(std::function<void(std::string, std::string)> callback) {
    Service::get().authorizeUpdates(std::move(callback));
}
void checkStaffAccount(std::function<void(bool)> done) {
    Service::get().checkStaffMember(std::move(done));
}
// 이미 방에 있으면 랭크 매치 여부에 따라 해당 화면으로, 아니면 홈 화면으로 이동한다.
void enterVersus() {
    if (Service::get().room()) {
        if (Service::get().isRankedMatch())
            showRanked();
        else
            showRoom();
    } else
        showVersus();
}
}
