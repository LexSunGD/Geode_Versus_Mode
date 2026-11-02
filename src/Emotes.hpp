// 이모트 목록과 퀵 슬롯 설정. 이모트 정의(kEmotes), 멤버십 필요 여부,
// 퀵 슬롯(5칸)의 저장/복원/정리를 담당한다. 그림과 연출은 EmoteArt.inl, EmoteEffects.inl.
#pragma once
#include <Geode/loader/Mod.hpp>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace versus::emotes {
// 이모트 하나. id는 서버와 주고받는 식별자, name은 표시 이름, member는 멤버십 전용 여부.
struct Emote {
    std::string_view id;
    std::string_view name;
    bool member;
};
// 전체 이모트 목록 (앞 6개는 누구나, 나머지는 멤버십 전용).
inline constexpr std::array<Emote, 40> kEmotes{{
    {"like", "Like", false},   {"dislike", "Dislike", false}, {"smile", "Smile", false},  {"sad", "Sad", false},
    {"angry", "Angry", false}, {"fire", "Fire", false},       {"money", "Money", true},   {"heart", "Heart", true},
    {"laugh", "Laugh", true},  {"cool", "Cool", true},        {"wow", "Wow", true},       {"cry", "Cry", true},
    {"love", "Love", true},    {"wink", "Wink", true},        {"tongue", "Tongue", true}, {"sweat", "Nervous", true},
    {"sleep", "Sleepy", true}, {"think", "Thinking", true},   {"skull", "Skull", true},   {"ghost", "Ghost", true},
    {"devil", "Devil", true},  {"angel", "Angel", true},      {"dizzy", "Dizzy", true},   {"party", "Party", true},
    {"crown", "Crown", true},  {"trophy", "Trophy", true},    {"star", "Star", true},     {"bolt", "Bolt", true},
    {"gg", "GG", true},        {"wp", "Well Played", true},   {"gl", "Good Luck", true},  {"lol", "LOL", true},
    {"hi", "Hi", true},        {"hundred", "100", true},      {"what", "What?!", true},   {"rip", "RIP", true},
    {"eyes", "Eyes", true},    {"cold", "Cold", true},        {"hot", "Hot", true},       {"swords", "Swords", true},
}};
// 퀵 슬롯 칸 수.
inline constexpr int kSlots = 5;
// 이모트 표시 유효 시간(ms). 전송 시각(서버 시간)이 이보다 오래된 이모트는 그리지 않는다.
inline constexpr long long kFreshMs = 5'000;
// 슬롯 기본값 (모두 무료 이모트).
inline constexpr std::array<std::string_view, kSlots> kDefaultSet{"like", "smile", "sad", "angry", "fire"};

// id로 이모트를 찾는다. 없으면 nullptr.
inline Emote const* find(std::string_view id) {
    for (auto const& emote : kEmotes)
        if (emote.id == id) return &emote;
    return nullptr;
}
// 멤버십 여부를 고려해 이 이모트를 쓸 수 있는지.
inline bool usable(std::string_view id, bool membership) {
    auto const* emote = find(id);
    return emote && (membership || !emote->member);
}
// 저장된 퀵 슬롯을 읽어 항상 kSlots개의 유효한 id로 만들어 돌려준다.
// 쓸 수 없거나 중복된 칸은 비우고, 빈 칸은 기본값 또는 안 쓰인 무료 이모트로 채운다.
inline std::vector<std::string> quickSet(bool membership) {
    std::vector<std::string> result;
    auto const saved = geode::Mod::get()->getSavedValue<std::string>("emote-set", "");
    size_t start = 0;
    while (start <= saved.size() && result.size() < kSlots) {
        auto end = saved.find(',', start);
        if (end == std::string::npos) end = saved.size();
        result.emplace_back(saved.substr(start, end - start));
        start = end + 1;
    }
    result.resize(kSlots);
    for (int i = 0; i < kSlots; ++i) {
        bool const duplicate = std::count(result.begin(), result.begin() + i, result[i]) > 0;
        if (!usable(result[i], membership) || duplicate) result[i].clear();
    }
    for (int i = 0; i < kSlots; ++i) {
        if (!result[i].empty()) continue;
        std::string pick(kDefaultSet[i]);
        if (std::find(result.begin(), result.end(), pick) != result.end()) {
            pick.clear();
            for (auto const& emote : kEmotes)
                if (!emote.member && std::find(result.begin(), result.end(), std::string(emote.id)) == result.end()) {
                    pick = std::string(emote.id);
                    break;
                }
        }
        result[i] = pick.empty() ? std::string(kDefaultSet[i]) : pick;
    }
    return result;
}
// 퀵 슬롯을 쉼표로 이어 저장한다.
inline void saveQuickSet(std::vector<std::string> const& set) {
    std::string joined;
    for (size_t i = 0; i < set.size() && i < kSlots; ++i) joined += (i ? "," : "") + set[i];
    geode::Mod::get()->setSavedValue<std::string>("emote-set", joined);
}
// 저장된 퀵 슬롯을 지워 기본값으로 되돌린다.
inline void resetQuickSet() {
    geode::Mod::get()->setSavedValue<std::string>("emote-set", "");
}
}
