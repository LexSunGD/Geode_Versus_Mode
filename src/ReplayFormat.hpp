// 리플레이 바이너리 포맷 (헤더 전용). 한 번의 시도(run)를 입력(Input)과 주기적 위치 샘플(Sample)로
// 기록한 Recording 구조와 이를 바이트열로 바꾸는 encode/decode를 담고 있다.
// 정수는 varint/zigzag, 실수는 직전 값과의 XOR 델타로 압축한다.
// 라이브 관전용으로는 Recording을 조각(LivePart)으로 잘라 보내고 받아 이어 붙이는 코드가 같이 있다.
// 디코더는 외부(서버)에서 온 데이터를 읽으므로 모든 범위를 검사하고 실패하면 nullopt를 돌려준다.
#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace versus::replay {

// 현재 인코딩 포맷 번호와 디코더가 읽을 수 있는 가장 오래된 포맷 번호 (4부터 시간 값 포함).
inline constexpr std::uint8_t kFormat = 4;
inline constexpr std::uint8_t kFirstFormat = 1;
// 샘플을 기록하는 기본 간격(step 단위).
inline constexpr int kSampleEvery = 4;
// 업로드 데이터 크기 상한 (바이트열 길이). 넘으면 샘플 간격을 늘려 다시 인코딩한다.
inline constexpr std::size_t kMaxUploadChars = 190'000;
// step 수 상한: 240 step/s로 30분. 디코드 때 범위 검사에 쓴다.
inline constexpr std::int32_t kMaxSteps = 240 * 60 * 30;
// 시간 값을 정수로 저장할 때의 단위 (1e-7초). kMaxTimeUnits는 허용하는 최댓값.
inline constexpr double kTimeUnit = 1e-7;
inline constexpr std::int64_t kMaxTimeUnits = 10'000'000'000'000;

// 시도가 끝난 방식: 사망 / 클리어 / 중간에 끊김(Cut).
enum class End : std::uint8_t { Death = 0, Clear = 1, Cut = 2 };

// 입력 한 건. step은 물리 step 번호, bits는 눌린 버튼 상태, time은 그 시각(초).
struct Input {
    std::int32_t step = 0;
    std::uint8_t bits = 0;
    double time = 0;
};

// Sample.flags 비트: 중력 반전, 지면 접촉, 입력 직후 샘플, 사망, 점프/좌/우 버튼 누름 상태.
enum SampleFlag : std::uint8_t {
    UpsideDown = 1,
    OnGround = 2,
    PostInput = 4,
    Dead = 8,
    HoldJump = 16,
    HoldLeft = 32,
    HoldRight = 64,
};

// 특정 step의 플레이어 상태 한 점 (player 0/1은 P1/P2). 재생 때 샘플 사이를 보간한다.
struct Sample {
    std::int32_t step = 0;
    std::uint8_t player = 0;
    std::uint8_t flags = 0;
    double x = 0, y = 0, yVelocity = 0, xVelocity = 0;
    float rotation = 0, lastX = 0, lastY = 0;
    double time = 0;
};

// 한 번의 시도 전체 기록. 레벨 식별(id, version, hash), 결과(percent, end), 난수 시드,
// 시간 정보(stepDelta, endTime), 입력/샘플 목록을 가진다. coins는 -1이면 정보 없음.
struct Recording {
    std::uint8_t format = kFormat;
    std::int64_t levelId = 0;
    int levelVersion = 0;
    std::uint64_t levelHash = 0;
    bool lowDetail = false;
    int run = 0, percent = 0;
    std::int32_t endStep = 0;
    End end = End::Death;
    // 기록 당시의 step 길이(초). 시간과 step을 서로 변환할 때 쓴다.
    float stepDelta = 0.f;
    std::uint64_t randomSeed = 0, replaySeed = 0;
    std::uint64_t seedBase = 0;
    double endTime = 0;
    // step 길이가 일정하지 않았음을 나타내는 표시 (true면 step과 time의 관계가 일정하지 않다).
    bool uneven = false;
    std::vector<Input> inputs;
    std::vector<Sample> samples;
    int coins = -1;
    // 시간 값이 들어 있는 포맷(4 이상)인지.
    bool timed() const { return format >= 4; }
};

// 바이트열 읽기/쓰기 도우미. 모든 정수는 little-endian 또는 varint 형식이다.
namespace detail {
// 바이트 버퍼에 쓴다. zigzag는 부호 있는 수를 varint에 효율적으로 넣기 위한 변환.
struct Writer {
    std::vector<std::uint8_t> out;
    void u8(std::uint8_t value) { out.push_back(value); }
    void varint(std::uint64_t value) {
        while (value >= 0x80) {
            out.push_back(static_cast<std::uint8_t>(value | 0x80));
            value >>= 7;
        }
        out.push_back(static_cast<std::uint8_t>(value));
    }
    void zigzag(std::int64_t value) {
        varint((static_cast<std::uint64_t>(value) << 1) ^ static_cast<std::uint64_t>(value >> 63));
    }
    void fixed64(std::uint64_t value) {
        for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
    void fixed32(std::uint32_t value) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
};
// 바이트 범위를 읽는다. 끝을 넘어 읽거나 varint가 너무 길면 ok가 false가 되고 0을 돌려준다.
struct Reader {
    std::span<std::uint8_t const> in;
    std::size_t at = 0;
    bool ok = true;
    std::uint8_t u8() {
        if (at >= in.size()) {
            ok = false;
            return 0;
        }
        return in[at++];
    }
    std::uint64_t varint() {
        std::uint64_t value = 0;
        for (int shift = 0; shift < 64; shift += 7) {
            auto const byte = u8();
            if (!ok) return 0;
            value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
            if (!(byte & 0x80)) return value;
        }
        ok = false;
        return 0;
    }
    std::int64_t zigzag() {
        auto const value = varint();
        return static_cast<std::int64_t>(value >> 1) ^ -static_cast<std::int64_t>(value & 1);
    }
    std::uint64_t fixed64() {
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i) value |= static_cast<std::uint64_t>(u8()) << (8 * i);
        return value;
    }
    std::uint32_t fixed32() {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(u8()) << (8 * i);
        return value;
    }
};
// 일반 리플레이 파일 식별자 "VRP" / double, float의 비트 표현.
inline constexpr std::uint8_t kMagic[3] = {'V', 'R', 'P'};
inline std::uint64_t bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
}
inline std::uint32_t bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}
}

// 레벨 문자열의 64비트 FNV-1a 해시. 리플레이가 같은 레벨 데이터로 찍혔는지 비교하는 데 쓴다.
inline std::uint64_t hashLevel(std::string_view data) {
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

// 초 <-> 시간 단위(정수) 변환. NaN/무한대는 0으로, 값은 범위를 잘라 넣는다.
// predictedUnits는 step 수와 stepDelta로 예상한 경과 시간이고, 실제와의 차이(residual)만 저장한다.
namespace detail {
inline std::int64_t timeUnits(double seconds) {
    return std::isfinite(seconds) ? std::llround(std::clamp(seconds, -1e6, 1e6) / kTimeUnit) : 0;
}
inline std::int64_t predictedUnits(std::int64_t steps, float stepDelta) {
    double const value = static_cast<double>(steps) * static_cast<double>(stepDelta) / kTimeUnit;
    return std::isfinite(value) ? std::llround(std::clamp(value, -1e13, 1e13)) : 0;
}
}

// 두 샘플 사이를 t(0~1)로 보간한다. 회전은 가까운 방향(최단 각도)으로 돈다.
inline Sample blend(Sample const& a, Sample const& b, double t) {
    Sample out = a;
    auto lerp = [t](double from, double to) { return from + (to - from) * t; };
    out.x = lerp(a.x, b.x);
    out.y = lerp(a.y, b.y);
    out.yVelocity = lerp(a.yVelocity, b.yVelocity);
    out.xVelocity = lerp(a.xVelocity, b.xVelocity);
    out.lastX = static_cast<float>(lerp(a.lastX, b.lastX));
    out.lastY = static_cast<float>(lerp(a.lastY, b.lastY));
    double turn = std::fmod(static_cast<double>(b.rotation) - a.rotation, 360.0);
    if (turn > 180.0)
        turn -= 360.0;
    else if (turn < -180.0)
        turn += 360.0;
    out.rotation = static_cast<float>(a.rotation + turn * t);
    return out;
}
// 두 샘플이 연속된 움직임인지 판정한다 (텔레포트/포탈 점프 등은 보간하지 않기 위함).
// 이동량이 초당 2400 단위 이내이고 중력 방향이 같아야 한다. 이 버전은 step 차이로, 아래는 time 차이로 시간을 잰다.
inline bool continuous(Sample const& a, Sample const& b, float recordedDelta) {
    double const seconds =
        std::max(1, b.step - a.step) * static_cast<double>(recordedDelta > 0.f ? recordedDelta : 1.f / 240.f);
    return std::abs(b.x - a.x) + std::abs(b.y - a.y) <= 2400.0 * seconds + 1.0 &&
           (a.flags & UpsideDown) == (b.flags & UpsideDown);
}
inline bool continuousTimed(Sample const& a, Sample const& b, float recordedDelta) {
    double const seconds =
        std::max(b.time - a.time, static_cast<double>(recordedDelta > 0.f ? recordedDelta : 1.f / 240.f));
    return std::abs(b.x - a.x) + std::abs(b.y - a.y) <= 2400.0 * seconds + 1.0 &&
           (a.flags & UpsideDown) == (b.flags & UpsideDown);
}

// 시간(초) -> step 변환과 그 반대. 주변 샘플 사이를 선형으로 보간하고 범위를 벗어나면 stepDelta로 외삽한다.
inline std::int32_t stepAtTime(Recording const& recording, double time) {
    auto const& samples = recording.samples;
    if (samples.empty()) return 0;
    auto it = std::upper_bound(samples.begin(), samples.end(), time + 1e-9,
                               [](double value, Sample const& sample) { return value < sample.time; });
    if (it == samples.begin()) return 0;
    auto const& a = *(it - 1);
    double step = a.step;
    if (it != samples.end() && it->time > a.time)
        step += std::floor((time - a.time) / (it->time - a.time) * (it->step - a.step) + 1e-6);
    else if (it == samples.end() && recording.stepDelta > 0.f)
        step += std::floor((time - a.time) / recording.stepDelta + 1e-6);
    return static_cast<std::int32_t>(std::clamp(step, 0.0, static_cast<double>(kMaxSteps)));
}
inline double timeAtStep(Recording const& recording, std::int32_t at) {
    auto const& samples = recording.samples;
    if (samples.empty()) return 0;
    auto it = std::upper_bound(samples.begin(), samples.end(), at,
                               [](std::int32_t value, Sample const& sample) { return value < sample.step; });
    if (it == samples.begin()) return 0;
    auto const& a = *(it - 1);
    if (it != samples.end() && it->step > a.step)
        return a.time + (it->time - a.time) * (at - a.step) / (it->step - a.step);
    return a.time + (at - a.step) * static_cast<double>(recording.stepDelta);
}
// 재생 중 샘플 목록을 앞으로만 훑는 커서. applied는 마지막으로 적용한 샘플 번호.
struct TimedCursor {
    std::size_t index = 0;
    std::int64_t applied = -1;
};
// 플레이어별 샘플 목록(list)에서 시각 at의 상태를 구한다. 커서를 앞으로 옮기고,
// 이미 적용한 샘플과 같은 구간이면 nullopt(새 상태 없음). 연속된 구간이면 다음 샘플과 보간한다.
inline std::optional<Sample> timedState(Recording const& recording, std::vector<std::uint32_t> const& list,
                                        TimedCursor& cursor, double at) {
    auto const& samples = recording.samples;
    if (list.empty()) return std::nullopt;
    auto& c = cursor.index;
    while (c + 1 < list.size() && samples[list[c + 1]].time <= at + 1e-9) ++c;
    auto const& a = samples[list[c]];
    if (a.time > at + 1e-9) return std::nullopt;
    if (cursor.applied == static_cast<std::int64_t>(c)) return std::nullopt;
    cursor.applied = static_cast<std::int64_t>(c);
    Sample state = a;
    if (c + 1 < list.size()) {
        auto const& b = samples[list[c + 1]];
        double const span = b.time - a.time;
        if (span > 1e-9 && continuousTimed(a, b, recording.stepDelta))
            state = blend(a, b, std::clamp((at - a.time) / span, 0.0, 1.0));
    }
    return state;
}
// time이 until보다 앞선 입력들을 건너뛰고 다음에 처리할 입력의 위치를 돌려준다.
inline std::size_t timedInputsUntil(Recording const& recording, std::size_t next, double until) {
    while (next < recording.inputs.size() && recording.inputs[next].time < until - 1e-9) ++next;
    return next;
}

// 샘플을 보관할지 결정한다. 첫 샘플과 간격 배수 step은 항상 보관하고,
// 입력 직후 샘플(PostInput)은 간격이 작거나 일부 step에서만 보관한다.
inline bool keepSample(Sample const& sample, int sampleInterval) {
    if (sample.step == 0 || sample.step % std::max(1, sampleInterval) == 0) return true;
    return (sample.flags & PostInput) && (sampleInterval <= 32 || sample.step % (sampleInterval >= 480 ? 16 : 4) == 0);
}
// 업로드가 너무 클 때 차례로 시도하는 샘플 간격 후보 (점점 성기게).
inline constexpr int kSampleIntervals[] = {4, 8, 16, 32, 64, 128, 240, 480, 960};

// Recording을 바이트열로 직렬화한다 (헤더 -> 입력 목록 -> 샘플 목록).
// 입력/샘플은 이전 항목과의 step 차이, 예상 시간 대비 잔차, XOR 델타로 저장해 크기를 줄인다.
inline std::vector<std::uint8_t> encode(Recording const& recording, int sampleInterval) {
    detail::Writer w;
    w.out.reserve(64 + recording.inputs.size() * 3 + recording.samples.size() * 30);
    for (auto byte : detail::kMagic) w.u8(byte);
    w.u8(kFormat);
    w.zigzag(recording.levelId);
    w.zigzag(recording.levelVersion);
    w.fixed64(recording.levelHash);
    w.u8(recording.lowDetail ? 1 : 0);
    w.varint(static_cast<std::uint64_t>(std::max(0, recording.run)));
    w.varint(static_cast<std::uint64_t>(std::clamp(recording.percent, 0, 100)));
    w.zigzag(recording.endStep);
    w.u8(static_cast<std::uint8_t>(recording.end));
    w.fixed32(detail::bits(recording.stepDelta));
    w.fixed64(recording.randomSeed);
    w.fixed64(recording.replaySeed);
    w.fixed64(recording.seedBase);
    w.u8(recording.uneven ? 1 : 0);
    w.zigzag(detail::timeUnits(recording.endTime));
    w.varint(recording.inputs.size());
    std::int32_t previous = 0;
    std::int64_t clock = 0;
    for (auto const& entry : recording.inputs) {
        w.zigzag(static_cast<std::int64_t>(entry.step) - previous);
        w.u8(entry.bits);
        std::int64_t const at = detail::timeUnits(entry.time);
        w.zigzag(at - clock -
                 detail::predictedUnits(static_cast<std::int64_t>(entry.step) - previous, recording.stepDelta));
        clock = at;
        previous = entry.step;
    }
    std::size_t kept = 0;
    for (auto const& sample : recording.samples)
        if (keepSample(sample, sampleInterval)) ++kept;
    w.varint(kept);
    previous = 0;
    clock = 0;
    std::uint64_t last64[2][4] = {};
    std::uint32_t last32[2][3] = {};
    for (auto const& sample : recording.samples) {
        if (!keepSample(sample, sampleInterval)) continue;
        int const p = sample.player ? 1 : 0;
        w.zigzag(static_cast<std::int64_t>(sample.step) - previous);
        std::int64_t const at = detail::timeUnits(sample.time);
        w.zigzag(at - clock -
                 detail::predictedUnits(static_cast<std::int64_t>(sample.step) - previous, recording.stepDelta));
        clock = at;
        previous = sample.step;
        w.u8(static_cast<std::uint8_t>(p));
        w.u8(sample.flags);
        std::uint64_t const wide[4] = {detail::bits(sample.x), detail::bits(sample.y), detail::bits(sample.yVelocity),
                                       detail::bits(sample.xVelocity)};
        for (int i = 0; i < 4; ++i) {
            w.varint(wide[i] ^ last64[p][i]);
            last64[p][i] = wide[i];
        }
        std::uint32_t const narrow[3] = {detail::bits(sample.rotation), detail::bits(sample.lastX),
                                         detail::bits(sample.lastY)};
        for (int i = 0; i < 3; ++i) {
            w.varint(narrow[i] ^ last32[p][i]);
            last32[p][i] = narrow[i];
        }
    }
    return std::move(w.out);
}

// 라이브 관전용 조각 포맷. LivePart는 Recording의 한 구간(index)으로, 첫 조각(head)에만 시드가 있고
// 마지막 조각(ended)에만 종료 정보가 있다. complete는 이 조각까지 데이터가 확정된 step.
inline constexpr std::uint8_t kLiveFormat = 1;
inline constexpr std::uint8_t kLiveMagic[3] = {'V', 'L', 'P'};
// 한 조각에 허용하는 입력/샘플 개수 상한 (디코드 방어용).
inline constexpr std::size_t kLiveMaxRecords = 200'000;
struct LivePart {
    int run = 0, index = 0;
    float stepDelta = 0.f;
    bool head = false;
    std::uint64_t randomSeed = 0, replaySeed = 0, seedBase = 0;
    bool uneven = false;
    std::int32_t complete = 0;
    double completeTime = 0;
    bool ended = false;
    End end = End::Cut;
    std::int32_t endStep = 0;
    double endTime = 0;
    int percent = 0;
    std::vector<Input> inputs;
    std::vector<Sample> samples;
};

// LivePart를 직렬화한다. 형식은 encode와 같은 델타 방식이고 헤더만 조각용이다.
inline std::vector<std::uint8_t> encodeLive(LivePart const& part) {
    detail::Writer w;
    w.out.reserve(48 + part.inputs.size() * 4 + part.samples.size() * 30);
    for (auto byte : kLiveMagic) w.u8(byte);
    w.u8(kLiveFormat);
    w.varint(static_cast<std::uint64_t>(std::max(0, part.run)));
    w.varint(static_cast<std::uint64_t>(std::max(0, part.index)));
    w.u8(static_cast<std::uint8_t>((part.head ? 1 : 0) | (part.ended ? 2 : 0) | (part.uneven ? 4 : 0)));
    w.fixed32(detail::bits(part.stepDelta));
    w.zigzag(part.complete);
    w.zigzag(detail::timeUnits(part.completeTime));
    if (part.head) {
        w.fixed64(part.randomSeed);
        w.fixed64(part.replaySeed);
        w.fixed64(part.seedBase);
    }
    if (part.ended) {
        w.u8(static_cast<std::uint8_t>(part.end));
        w.zigzag(part.endStep);
        w.zigzag(detail::timeUnits(part.endTime));
        w.varint(static_cast<std::uint64_t>(std::clamp(part.percent, 0, 100)));
    }
    w.varint(part.inputs.size());
    std::int32_t previous = 0;
    std::int64_t clock = 0;
    for (auto const& entry : part.inputs) {
        w.zigzag(static_cast<std::int64_t>(entry.step) - previous);
        w.u8(entry.bits);
        std::int64_t const at = detail::timeUnits(entry.time);
        w.zigzag(at - clock - detail::predictedUnits(static_cast<std::int64_t>(entry.step) - previous, part.stepDelta));
        clock = at;
        previous = entry.step;
    }
    w.varint(part.samples.size());
    previous = 0;
    clock = 0;
    std::uint64_t last64[2][4] = {};
    std::uint32_t last32[2][3] = {};
    for (auto const& sample : part.samples) {
        int const p = sample.player ? 1 : 0;
        w.zigzag(static_cast<std::int64_t>(sample.step) - previous);
        std::int64_t const at = detail::timeUnits(sample.time);
        w.zigzag(at - clock -
                 detail::predictedUnits(static_cast<std::int64_t>(sample.step) - previous, part.stepDelta));
        clock = at;
        previous = sample.step;
        w.u8(static_cast<std::uint8_t>(p));
        w.u8(sample.flags);
        std::uint64_t const wide[4] = {detail::bits(sample.x), detail::bits(sample.y), detail::bits(sample.yVelocity),
                                       detail::bits(sample.xVelocity)};
        for (int i = 0; i < 4; ++i) {
            w.varint(wide[i] ^ last64[p][i]);
            last64[p][i] = wide[i];
        }
        std::uint32_t const narrow[3] = {detail::bits(sample.rotation), detail::bits(sample.lastX),
                                         detail::bits(sample.lastY)};
        for (int i = 0; i < 3; ++i) {
            w.varint(narrow[i] ^ last32[p][i]);
            last32[p][i] = narrow[i];
        }
    }
    return std::move(w.out);
}

// LivePart를 읽는다. 매직/포맷/범위가 하나라도 맞지 않거나 남는 바이트가 있으면 nullopt.
inline std::optional<LivePart> decodeLive(std::span<std::uint8_t const> data) {
    detail::Reader r{data};
    for (auto byte : kLiveMagic)
        if (r.u8() != byte) return std::nullopt;
    if (r.u8() != kLiveFormat) return std::nullopt;
    LivePart part;
    auto const run = r.varint(), index = r.varint();
    if (run < 1 || run > 1'000'000 || index > 1'000'000) return std::nullopt;
    part.run = static_cast<int>(run);
    part.index = static_cast<int>(index);
    auto const flags = r.u8();
    if (flags > 7) return std::nullopt;
    part.head = flags & 1;
    part.ended = flags & 2;
    part.uneven = flags & 4;
    part.stepDelta = std::bit_cast<float>(r.fixed32());
    if (!std::isfinite(part.stepDelta) || part.stepDelta < 0.f || part.stepDelta > 1.f) return std::nullopt;
    auto const complete = r.zigzag();
    if (complete < 0 || complete > kMaxSteps + 1) return std::nullopt;
    part.complete = static_cast<std::int32_t>(complete);
    auto const completeTime = r.zigzag();
    if (completeTime < 0 || completeTime > kMaxTimeUnits) return std::nullopt;
    part.completeTime = static_cast<double>(completeTime) * kTimeUnit;
    if (part.head) {
        part.randomSeed = r.fixed64();
        part.replaySeed = r.fixed64();
        part.seedBase = r.fixed64();
    }
    if (part.ended) {
        auto const end = r.u8();
        if (end > 2) return std::nullopt;
        part.end = static_cast<End>(end);
        auto const endStep = r.zigzag();
        if (endStep < 0 || endStep > kMaxSteps) return std::nullopt;
        part.endStep = static_cast<std::int32_t>(endStep);
        auto const endTime = r.zigzag();
        if (endTime < -kMaxTimeUnits || endTime > kMaxTimeUnits) return std::nullopt;
        part.endTime = static_cast<double>(endTime) * kTimeUnit;
        part.percent = static_cast<int>(std::min<std::uint64_t>(r.varint(), 100));
    }
    auto const inputs = r.varint();
    if (!r.ok || inputs > kLiveMaxRecords || inputs > data.size()) return std::nullopt;
    part.inputs.reserve(inputs);
    std::int64_t step = 0, clock = 0;
    for (std::uint64_t i = 0; i < inputs && r.ok; ++i) {
        auto const gap = r.zigzag();
        step += gap;
        if (step < 0 || step > kMaxSteps) return std::nullopt;
        Input entry{static_cast<std::int32_t>(step), r.u8()};
        auto const residual = r.zigzag();
        if (residual < -kMaxTimeUnits || residual > kMaxTimeUnits) return std::nullopt;
        clock += detail::predictedUnits(gap, part.stepDelta) + residual;
        if (clock < -kMaxTimeUnits || clock > kMaxTimeUnits) return std::nullopt;
        entry.time = static_cast<double>(clock) * kTimeUnit;
        part.inputs.push_back(entry);
    }
    auto const samples = r.varint();
    if (!r.ok || samples > kLiveMaxRecords || samples > data.size()) return std::nullopt;
    part.samples.reserve(samples);
    step = 0;
    clock = 0;
    std::uint64_t last64[2][4] = {};
    std::uint32_t last32[2][3] = {};
    for (std::uint64_t i = 0; i < samples && r.ok; ++i) {
        Sample sample;
        auto const gap = r.zigzag();
        step += gap;
        if (step < 0 || step > kMaxSteps) return std::nullopt;
        sample.step = static_cast<std::int32_t>(step);
        auto const residual = r.zigzag();
        if (residual < -kMaxTimeUnits || residual > kMaxTimeUnits) return std::nullopt;
        clock += detail::predictedUnits(gap, part.stepDelta) + residual;
        if (clock < -kMaxTimeUnits || clock > kMaxTimeUnits) return std::nullopt;
        sample.time = static_cast<double>(clock) * kTimeUnit;
        int const p = r.u8() ? 1 : 0;
        sample.player = static_cast<std::uint8_t>(p);
        sample.flags = r.u8();
        for (int k = 0; k < 4; ++k) last64[p][k] ^= r.varint();
        for (int k = 0; k < 3; ++k) last32[p][k] ^= static_cast<std::uint32_t>(r.varint());
        sample.x = std::bit_cast<double>(last64[p][0]);
        sample.y = std::bit_cast<double>(last64[p][1]);
        sample.yVelocity = std::bit_cast<double>(last64[p][2]);
        sample.xVelocity = std::bit_cast<double>(last64[p][3]);
        sample.rotation = std::bit_cast<float>(last32[p][0]);
        sample.lastX = std::bit_cast<float>(last32[p][1]);
        sample.lastY = std::bit_cast<float>(last32[p][2]);
        part.samples.push_back(sample);
    }
    if (!r.ok || r.at != data.size()) return std::nullopt;
    return part;
}

// 받은 조각을 Recording 뒤에 이어 붙인다. run이 다르거나 이미 받은 구간보다
// 앞선 데이터가 오면 거부(false)한다. head 조각의 시드와 ended 조각의 종료 정보를 반영한다.
inline bool appendLive(Recording& recording, LivePart const& part) {
    if (part.run != recording.run) return false;
    auto const lastInput = recording.inputs.empty() ? -1 : recording.inputs.back().step;
    auto const lastSample = recording.samples.empty() ? -1 : recording.samples.back().step;
    if (!part.inputs.empty() && part.inputs.front().step < lastInput) return false;
    if (!part.samples.empty() && part.samples.front().step < lastSample) return false;
    if (part.stepDelta > 0.f) recording.stepDelta = part.stepDelta;
    if (part.head) {
        recording.randomSeed = part.randomSeed;
        recording.replaySeed = part.replaySeed;
        recording.seedBase = part.seedBase;
    }
    recording.uneven = recording.uneven || part.uneven;
    recording.inputs.insert(recording.inputs.end(), part.inputs.begin(), part.inputs.end());
    recording.samples.insert(recording.samples.end(), part.samples.begin(), part.samples.end());
    if (part.ended) {
        recording.end = part.end;
        recording.endStep = part.endStep;
        recording.endTime = part.endTime;
        recording.percent = part.percent;
    }
    return true;
}

// Recording에서 조각을 잘라 낼 때 어디까지 보냈는지 기억하는 커서.
struct LiveCursor {
    int run = 0, index = 0;
    std::size_t inputs = 0, samples = 0;
    std::int32_t complete = 0;
    bool ended = false;
};
// source에서 cursor 다음부터 bound step 앞까지의 입력/샘플을 한 조각으로 잘라 낸다.
// 조각이 maxSamples/maxInputs를 넘지 않도록 cut 위치를 앞당기고, 새 정보가 없으면 nullopt.
// ending이고 남은 데이터를 모두 담았으면 종료 정보를 붙인 마지막 조각이 된다.
inline std::optional<LivePart> cutLivePart(Recording const& source, LiveCursor& cursor, std::int32_t bound,
                                           double boundTime, bool ending, int interval = 8,
                                           std::size_t maxSamples = 1000, std::size_t maxInputs = 4000) {
    if (cursor.ended) return std::nullopt;
    auto const& samples = source.samples;
    auto const& inputs = source.inputs;
    std::size_t e = cursor.samples, kept = 0;
    while (e < samples.size() && samples[e].step < bound) {
        if (keepSample(samples[e], interval) && ++kept > maxSamples) break;
        ++e;
    }
    std::int32_t cut = bound;
    double cutTime = boundTime;
    if (e < samples.size() && samples[e].step < bound) {
        cut = samples[e].step;
        cutTime = samples[e].time;
    }
    std::size_t i = cursor.inputs;
    while (i < inputs.size() && inputs[i].step < cut) {
        if (i - cursor.inputs == maxInputs) {
            cut = inputs[i].step;
            cutTime = inputs[i].time;
            break;
        }
        ++i;
    }
    while (i > cursor.inputs && inputs[i - 1].step >= cut) --i;
    while (e > cursor.samples && samples[e - 1].step >= cut) --e;
    bool const last = ending && e == samples.size() && i == inputs.size();
    if (cursor.index > 0 && !last && cut <= cursor.complete) return std::nullopt;
    LivePart part;
    part.run = cursor.run;
    part.index = cursor.index;
    part.stepDelta = source.stepDelta;
    part.head = part.index == 0;
    part.randomSeed = source.randomSeed;
    part.replaySeed = source.replaySeed;
    part.seedBase = source.seedBase;
    part.uneven = source.uneven;
    part.complete = std::max(cut, cursor.complete);
    part.completeTime = std::max(0.0, cutTime);
    for (std::size_t k = cursor.samples; k < e; ++k)
        if (keepSample(samples[k], interval)) part.samples.push_back(samples[k]);
    part.inputs.assign(inputs.begin() + static_cast<std::ptrdiff_t>(cursor.inputs),
                       inputs.begin() + static_cast<std::ptrdiff_t>(i));
    if (last) {
        part.ended = true;
        part.end = source.end;
        part.endStep = source.endStep;
        part.endTime = source.endTime;
        part.percent = source.percent;
    }
    cursor.samples = e;
    cursor.inputs = i;
    cursor.complete = part.complete;
    cursor.ended = last;
    ++cursor.index;
    return part;
}

// 바이트열을 Recording으로 읽는다. 포맷 1~4를 지원하고, 포맷 3부터 seedBase, 4부터 시간 값이 있다.
// 개수와 값 범위를 모두 검사하며 하나라도 어긋나면 nullopt.
inline std::optional<Recording> decode(std::span<std::uint8_t const> data) {
    detail::Reader r{data};
    for (auto byte : detail::kMagic)
        if (r.u8() != byte) return std::nullopt;
    auto const format = r.u8();
    if (format < kFirstFormat || format > kFormat) return std::nullopt;
    Recording recording;
    recording.format = format;
    recording.levelId = r.zigzag();
    recording.levelVersion = static_cast<int>(r.zigzag());
    recording.levelHash = r.fixed64();
    recording.lowDetail = r.u8() != 0;
    recording.run = static_cast<int>(std::min<std::uint64_t>(r.varint(), 1'000'000));
    recording.percent = static_cast<int>(std::min<std::uint64_t>(r.varint(), 100));
    recording.endStep = static_cast<std::int32_t>(std::clamp<std::int64_t>(r.zigzag(), 0, kMaxSteps));
    auto const end = r.u8();
    if (end > 2) return std::nullopt;
    recording.end = static_cast<End>(end);
    recording.stepDelta = std::bit_cast<float>(r.fixed32());
    recording.randomSeed = r.fixed64();
    recording.replaySeed = r.fixed64();
    if (format >= 3) recording.seedBase = r.fixed64();
    bool const timed = format >= 4;
    if (timed) {
        auto const uneven = r.u8();
        if (uneven > 1) return std::nullopt;
        recording.uneven = uneven == 1;
        recording.endTime = static_cast<double>(r.zigzag()) * kTimeUnit;
    }
    auto const inputs = r.varint();
    if (!r.ok || inputs > 2'000'000 || inputs > data.size()) return std::nullopt;
    recording.inputs.reserve(inputs);
    std::int64_t step = 0, clock = 0;
    for (std::uint64_t i = 0; i < inputs && r.ok; ++i) {
        auto const gap = r.zigzag();
        step += gap;
        if (step < 0 || step > kMaxSteps) return std::nullopt;
        Input entry{static_cast<std::int32_t>(step), r.u8()};
        if (timed) {
            auto const residual = r.zigzag();
            if (residual < -kMaxTimeUnits || residual > kMaxTimeUnits) return std::nullopt;
            clock += detail::predictedUnits(gap, recording.stepDelta) + residual;
            if (clock < -kMaxTimeUnits || clock > kMaxTimeUnits) return std::nullopt;
            entry.time = static_cast<double>(clock) * kTimeUnit;
        }
        recording.inputs.push_back(entry);
    }
    auto const samples = r.varint();
    if (!r.ok || samples > 5'000'000 || samples > data.size()) return std::nullopt;
    recording.samples.reserve(samples);
    step = 0;
    clock = 0;
    std::uint64_t last64[2][4] = {};
    std::uint32_t last32[2][3] = {};
    for (std::uint64_t i = 0; i < samples && r.ok; ++i) {
        Sample sample;
        auto const gap = r.zigzag();
        step += gap;
        if (step < 0 || step > kMaxSteps) return std::nullopt;
        sample.step = static_cast<std::int32_t>(step);
        if (timed) {
            auto const residual = r.zigzag();
            if (residual < -kMaxTimeUnits || residual > kMaxTimeUnits) return std::nullopt;
            clock += detail::predictedUnits(gap, recording.stepDelta) + residual;
            if (clock < -kMaxTimeUnits || clock > kMaxTimeUnits) return std::nullopt;
            sample.time = static_cast<double>(clock) * kTimeUnit;
        }
        int const p = r.u8() ? 1 : 0;
        sample.player = static_cast<std::uint8_t>(p);
        sample.flags = r.u8();
        for (int k = 0; k < 4; ++k) last64[p][k] ^= r.varint();
        for (int k = 0; k < 3; ++k) last32[p][k] ^= static_cast<std::uint32_t>(r.varint());
        sample.x = std::bit_cast<double>(last64[p][0]);
        sample.y = std::bit_cast<double>(last64[p][1]);
        sample.yVelocity = std::bit_cast<double>(last64[p][2]);
        sample.xVelocity = std::bit_cast<double>(last64[p][3]);
        sample.rotation = std::bit_cast<float>(last32[p][0]);
        sample.lastX = std::bit_cast<float>(last32[p][1]);
        sample.lastY = std::bit_cast<float>(last32[p][2]);
        recording.samples.push_back(sample);
    }
    if (!r.ok || r.at != data.size()) return std::nullopt;
    return recording;
}

}
