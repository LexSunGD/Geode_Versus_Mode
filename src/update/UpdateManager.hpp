// 업데이트 확인, 다운로드, 설치 흐름을 관리하는 싱글턴 UpdateManager 선언.
// 상태는 mutex로 보호되는 UpdateSnapshot으로 UI에 전달한다. 구현은 UpdateManager.cpp.
#pragma once
#include "UpdateInstaller.hpp"
#include "UpdatePatch.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

namespace versus::update {
// 업데이트 진행 단계.
enum class UpdateState {
    Idle,
    Checking,
    Current,
    UpdateAvailable,
    Downloading,
    Verifying,
    ReadyToInstall,
    VerifiedDownloaded,
    Installing,
    Restarting,
    Error
};
// UI가 읽는 상태 스냅샷: 단계, 매니페스트, 설치된 버전/채널, 메시지, 다운로드 진행(바이트,
// 속도),
// 서버보다 설치된 버전이 더 새로운지 여부.
struct UpdateSnapshot {
    UpdateState state = UpdateState::Idle;
    Manifest manifest;
    std::string installedVersion, installedChannel, message, downloadedPath;
    uint64_t downloaded = 0;
    uint64_t transferTotal = 0, transferDone = 0;
    double bytesPerSecond = 0;
    bool busy = false;
    bool newerThanServer = false;
};
// 업데이트 상태 머신. 서버 통신은 백그라운드에서, 상태 갱신은 m_mutex로 보호한다.
// m_cancel은 다운로드 일시정지 요청 플래그이다.
class UpdateManager {
    std::mutex m_mutex;
    UpdateSnapshot m_snapshot;
    std::atomic_bool m_cancel{false};
    InstallContext m_context;
    std::string m_token;
    bool m_installAfterRecords = false;
    bool m_creator = false;
    std::chrono::steady_clock::time_point m_recordsSince;
    std::string m_baseDigest, m_unifiedOnly;
    uint64_t m_baseSize = 0;
    fs::file_time_type m_baseTime{};
    // 내부 함수: 상태 전환, 실패 처리, 인증 토큰 받기, 확인 수행, 패키지 검증, 패치 계획 읽기,
    // 패키지 받기.
    void state(UpdateState value, std::string message = {});
    void fail(std::string message);
    void authorize();
    void performCheck(std::string token);
    void validateArchive(fs::path const&, Manifest const&);
    std::optional<PatchPlan> loadPatch(Manifest const&);
    fs::path fetchPackage(Manifest const&);

public:
    // 공개 인터페이스: 확인/다운로드/설치/일시정지.
    // waitingForRecords는 기록 전송이 끝나기를 기다리는 중인지, restartTimeout은 재시작 대기
    // 시간 초과 처리.
    static UpdateManager& get();
    UpdateSnapshot snapshot();
    void check();
    void download();
    void install();
    bool waitingForRecords();
    void restartTimeout();
    void pause();
};
// 업데이트 확인 화면을 연다.
void showUpdateCheck();
}
