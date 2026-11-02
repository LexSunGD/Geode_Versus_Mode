// SpectatorRunner(매치메이킹 대기 중 즐기는 미니 러너 게임)가 쓰는 물리 규칙 모음.
// 프레임 보정(runnerFrameDelta), 점수 구간 달성 판정(RunnerMilestones), 점프 물리(RunnerJump).
#pragma once
#include <algorithm>
#include <cmath>
namespace versus {
// 프레임 시간 보정: 유한하지 않은 값은 0, 그 외에는 0~0.05초로 제한한 뒤 1.2배로 가속한다.
inline float runnerFrameDelta(float dt) {
    return std::isfinite(dt) ? std::clamp(dt, 0.f, .05f) * 1.2f : 0.f;
}
// 점수 300점마다 한 단계씩 올라가는 구간 달성 판정.
struct RunnerMilestones {
    static constexpr int INTERVAL = 300;
    int stage = 0;
    // 점수가 속한 구간이 이전보다 올라갔을 때만 true를 돌려준다.
    bool update(int score) {
        int next = std::max(0, score) / INTERVAL;
        bool reached = next > stage;
        stage = next;
        return reached;
    }
};
// 러너 점프 상태. 누르고 있는 동안 중력이 약해져 더 높이 뛴다.
struct RunnerJump {
    float height = 0.f, velocity = 0.f, heldFor = 0.f;
    bool held = false;
    // 땅에 있을 때만 초기 속도를 주고 점프를 시작한다. 이미 누른 상태면 무시한다.
    void press() {
        if (held) return;
        held = true;
        if (height <= 0.f) {
            velocity = 265.f;
            heldFor = 0.f;
        }
    }
    // 버튼을 떼면 상승 속도를 제한해 낮은 점프가 되게 한다.
    void release() {
        held = false;
        if (velocity > 120.f) velocity = 120.f;
    }
    // dt를 1/60초로 제한해 적분한다. 누른 채 0.24초 안에 상승 중이면 중력을 낮게 적용한다.
    void step(float dt) {
        if (!std::isfinite(dt) || dt <= 0.f) return;
        dt = std::clamp(dt, 0.f, 1.f / 60.f);
        if (height <= 0.f && velocity <= 0.f) return;
        heldFor += dt;
        float gravity = held && heldFor < .24f && velocity > 0.f ? 430.f : 1000.f;
        velocity -= gravity * dt;
        height += velocity * dt;
        if (height < 0.f) {
            height = 0.f;
            velocity = 0.f;
        }
    }
};
}
