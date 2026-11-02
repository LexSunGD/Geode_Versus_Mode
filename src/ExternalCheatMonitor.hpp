// 다른 모드를 통한 외부 치트 검사의 공개 인터페이스. 구현은 ExternalCheatMonitor.cpp.
// begin/end 함수들은 Prism, Mega Hack 훅이 호출 구간을 알려주는 용도이다.
#pragma once

#include <string>
#include <vector>

class PlayLayer;
namespace cocos2d {
class CCScheduler;
}

namespace versus::integrity {

// 검사 결과. code는 감지 종류, source는 모드 이름, detail은 설명, features는 인코딩된 기능
// 목록.
struct ExternalCheatStatus {
    bool detected = false;
    std::string code;
    std::string source;
    std::string detail;
    std::string features;
};

// 감지된 첫 번째 외부 치트. includeAttemptHistory는 현재 구현에서 쓰이지 않는다.
ExternalCheatStatus activeExternalCheat(bool includeAttemptHistory);
// 감지된 모든 외부 치트.
std::vector<ExternalCheatStatus> activeExternalCheats(bool includeAttemptHistory);

// Prism의 입력 처리 구간(Prism / 게임 본래 구간)을 표시하고, 주입된 입력을 관측한다.
void beginPrismProcessScope();
void endPrismProcessScope();
void beginNativeProcessScope();
void endNativeProcessScope();
ExternalCheatStatus observePrismInjectedInput();

// PlayLayer 초기화 구간 표시와 Prism의 Instant Complete 관측.
void beginPrismInitScope();
void markPrismInitBaseReturned();
void endPrismInitScope();
void observePrismInstantComplete(PlayLayer* layer);
// Prism의 CCScheduler 구간 표시와 dt 변경(배속) 관측.
void beginPrismSchedulerScope(cocos2d::CCScheduler* scheduler, double dt);
void endPrismSchedulerScope(cocos2d::CCScheduler* scheduler);
ExternalCheatStatus observePrismSchedulerDelta(cocos2d::CCScheduler* scheduler, double dt);

// Mega Hack의 postUpdate 구간 표시와 Instant Complete 관측.
void beginMegaHackPostUpdateScope();
void markMegaHackPostUpdateBaseReturned();
void endMegaHackPostUpdateScope();
void observeMegaHackInstantComplete(PlayLayer* layer);
// 배속 변경이 관측됐을 때 어느 모드의 Speedhack인지 판별한다.
ExternalCheatStatus attributeRuntimeSpeedhack();

}
