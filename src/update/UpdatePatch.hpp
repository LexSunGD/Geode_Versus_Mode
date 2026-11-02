// 서버가 주는 JSON(플랫폼별 패키지 정보, 패치 계획)을 읽어 Manifest와 PatchPlan으로 바꾸는
// 코드.
// 모든 값은 형식과 범위를 검증하고, 잘못되면 거부한다.
#pragma once
#include "UpdateCore.hpp"
#include <cmath>
#include <matjson.hpp>

namespace versus::update {
// 패치 계획 JSON의 최대 크기(1MB).
inline constexpr uint64_t MAX_PLAN_SIZE = 1024 * 1024;

// JSON에서 0 이상의 정수 값을 읽는다(2^53 이하). 아니면 nullopt.
inline std::optional<uint64_t> jsonSize(matjson::Value const& value, std::string_view key) {
    auto field = value.get(key);
    if (!field.isOk() || !field.unwrap().isNumber()) return std::nullopt;
    auto number = field.unwrap().asDouble().unwrapOr(-1);
    if (!std::isfinite(number) || number < 0 || number > 9007199254740992. || std::floor(number) != number)
        return std::nullopt;
    return static_cast<uint64_t>(number);
}
// JSON에서 문자열 값을 읽는다.
inline std::optional<std::string> jsonText(matjson::Value const& value, std::string_view key) {
    auto field = value.get(key);
    if (!field.isOk() || !field.unwrap().isString()) return std::nullopt;
    return field.unwrap().asString().unwrap();
}

// 매니페스트의 platforms 항목에서 현재 플랫폼의 패키지 정보를 반영한다.
// 기존 통합 패키지 정보는 unified*에 보관하고, 검증을 통과할 때만 적용한다.
inline bool adoptPlatform(Manifest& m, matjson::Value const& manifest, std::string const& platform) {
    try {
        if (!platformValid(platform) || !m.platform.empty()) return false;
        auto platforms = manifest.get("platforms");
        if (!platforms.isOk() || !platforms.unwrap().isObject()) return false;
        auto entry = platforms.unwrap().get(platform);
        if (!entry.isOk() || !entry.unwrap().isObject()) return false;
        auto url = jsonText(entry.unwrap(), "url"), sha256 = jsonText(entry.unwrap(), "sha256");
        auto size = jsonSize(entry.unwrap(), "size");
        if (!url || !sha256 || !size) return false;
        Manifest candidate = m;
        candidate.platform = platform;
        candidate.unifiedUrl = m.url;
        candidate.unifiedSha256 = m.sha256;
        candidate.unifiedSize = m.size;
        candidate.url = *url;
        candidate.sha256 = *sha256;
        candidate.size = *size;
        ManifestVerifier().validate(candidate);
        m = std::move(candidate);
        return true;
    } catch (std::exception const&) {
        return false;
    }
}

// 패치 계획 JSON의 URL을 만든다. 파일명에 설치된 패키지 해시 앞 16자를 넣고,
// 패키지 URL의 쿼리(ticket)를 그대로 붙이며, 허용된 주소일 때만 반환한다.
// GitHub 릴리스는 패치 계획을 제공하지 않으므로 항상 빈 문자열(전체 다운로드)이다.
inline std::string patchUrl(Manifest const&, std::string const&) {
    return {};
}

// 패치 계획 JSON을 PatchPlan으로 바꾼다. 스키마 1, 버전/플랫폼, 목표 패키지와 기존
// 패키지의 크기/해시가
// 일치해야 한다. 조각은 ["c", 오프셋, 길이], ["f", 길이], ["l", 16진수 바이트] 형식이며
// 마지막에 validatePlan으로 확인한다.
inline std::optional<PatchPlan> parsePatchPlan(matjson::Value const& value, Manifest const& m, fs::path const& base,
                                               uint64_t baseSize, std::string const& baseDigest) {
    try {
        if (!value.isObject() || jsonSize(value, "schema") != uint64_t(1)) return std::nullopt;
        if (jsonText(value, "version") != m.version || jsonText(value, "platform") != m.platform) return std::nullopt;
        auto target = value.get("target"), from = value.get("base"), segments = value.get("segments");
        if (!target.isOk() || !from.isOk() || !segments.isOk() || !segments.unwrap().isArray()) return std::nullopt;
        if (jsonSize(target.unwrap(), "size") != m.size || jsonText(target.unwrap(), "sha256") != m.sha256)
            return std::nullopt;
        if (jsonSize(from.unwrap(), "size") != baseSize || jsonText(from.unwrap(), "sha256") != baseDigest)
            return std::nullopt;
        PatchPlan plan;
        plan.base = base;
        plan.baseSize = baseSize;
        for (auto const& item : segments.unwrap()) {
            if (!item.isArray() || item.size() < 2 || !item[0].isString()) return std::nullopt;
            auto kind = item[0].asString().unwrap();
            PatchSegment segment;
            auto number = [&](size_t index) -> std::optional<uint64_t> {
                if (index >= item.size() || !item[index].isNumber()) return std::nullopt;
                auto n = item[index].asDouble().unwrapOr(-1);
                if (!std::isfinite(n) || n < 0 || n > double(MAX_SIZE) || std::floor(n) != n) return std::nullopt;
                return static_cast<uint64_t>(n);
            };
            if (kind == "c" && item.size() == 3) {
                auto offset = number(1), length = number(2);
                if (!offset || !length) return std::nullopt;
                segment.kind = 'c';
                segment.baseOffset = *offset;
                segment.length = *length;
            } else if (kind == "f" && item.size() == 2) {
                auto length = number(1);
                if (!length) return std::nullopt;
                segment.kind = 'f';
                segment.length = *length;
            } else if (kind == "l" && item.size() == 2 && item[1].isString()) {
                auto bytes = hexBytes(item[1].asString().unwrap());
                if (!bytes) return std::nullopt;
                segment.kind = 'l';
                segment.length = bytes->size();
                segment.bytes = std::move(*bytes);
            } else
                return std::nullopt;
            plan.segments.push_back(std::move(segment));
        }
        validatePlan(plan, m);
        return plan;
    } catch (std::exception const&) {
        return std::nullopt;
    }
}
}
