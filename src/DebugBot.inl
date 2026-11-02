// 디버그 빌드 전용 봇 배틀. 서버 없이 로컬에서 가상의 상대와 랭크 스타일 1v1 을 시험한다.
// 봇은 미리 짠 시도 계획(botPlan)대로 시간에 맞춰 퍼센트를 올리며, 결과는 저장되지 않고
// 레이팅도 변하지 않는다. Service 구현에 포함되는 파일.
namespace {
// 봇의 최고 기록(퍼센트). 계획의 한 시도만 이 값에 도달한다.
constexpr int kBotBest = 10;

// 맵 난이도(별/데몬 난이도)와 길이에 따라 봇 배틀의 시도(목숨) 횟수를 정한다.
// Long/XL 맵일수록 줄이고, 플레이 시간이 긴 XL 맵은 더 줄인다.
int botAttempts(LevelInfo const& level) {
    static constexpr std::array<std::pair<int, int>, 8> stars{
        {{2, 3}, {3, 3}, {4, 4}, {5, 4}, {6, 6}, {7, 7}, {8, 9}, {9, 11}}};
    static constexpr std::array<std::pair<int, int>, 5> demons{{{7, 12}, {8, 15}, {6, 18}, {9, 21}, {10, 25}}};
    int base = 4;
    if (level.demon) {
        for (auto [difficulty, lives] : demons)
            if (difficulty == level.difficulty) base = lives;
    } else
        for (auto [count, lives] : stars)
            if (count == level.stars) base = lives;
    double const scale = level.length == 4 ? .5 : level.length == 3 ? .7 : 1.;
    if (level.length == 4 && mapPlaySeconds(level.id) > 180.) return std::max(2, static_cast<int>(base * .3 + .5));
    return std::max(3, static_cast<int>(base * scale + .5));
}

// 봇의 시도별 계획 {걸리는 초, 도달 퍼센트}를 만든다.
// 무작위 한 시도만 최고 기록에 도달하고, 나머지는 더 낮은 퍼센트에서 끝난다.
std::vector<std::pair<float, int>> botPlan(int lives) {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> early(2, kBotBest - 1);
    std::uniform_real_distribution<float> extra(0.f, 1.6f);
    int const best = std::uniform_int_distribution<int>(0, std::max(0, lives - 1))(rng);
    std::vector<std::pair<float, int>> plan;
    for (int i = 0; i < lives; ++i) {
        int const percent = i == best ? kBotBest : early(rng);
        plan.push_back({1.4f + percent * .38f + extra(rng), percent});
    }
    return plan;
}

// 현재 방의 배틀 상태를 서버와 같은 규칙(resolveBattle)으로 판정해, 끝났으면
// 종료 시각·승자·무승부를 기록하고 결과 메시지를 만든다.
void botResolve() {
    auto& s = state();
    if (!s.room || !s.room->battle || !s.room->guest || s.room->battle->finishedAt > 0) return;
    auto& room = *s.room;
    auto& battle = *room.battle;
    auto body = Json::object();
    auto host = Json::object();
    host["uid"] = room.host.uid;
    body["host"] = std::move(host);
    auto guest = Json::object();
    guest["uid"] = room.guest->uid;
    body["guest"] = std::move(guest);
    body["rules"] = rulesJson(room.rules);
    auto value = Json::object();
    value["host"] = battlePlayerJson(battle.host);
    value["guest"] = battlePlayerJson(battle.guest);
    value["finishedAt"] = 0;
    if (!battle.firstReachedUid.empty()) value["firstReachedUid"] = battle.firstReachedUid;
    body["battle"] = std::move(value);
    resolveBattle(body, room.host.uid);
    auto const& out = std::as_const(body)["battle"];
    if (!out["finishedAt"].isObject() && intAt(out, "finishedAt") <= 0) return;
    battle.finishedAt = nowMs();
    battle.draw = boolAt(out, "draw");
    battle.winnerUid = stringAt(out, "winnerUid");
    bool const won = !battle.draw && battle.winnerUid == room.host.uid;
    s.botResult =
        fmt::format("Bot battle: {} vs {} ({}% - {}%). No rating change, nothing was saved.",
                    battle.draw ? "DRAW"
                    : won       ? "VICTORY"
                                : "DEFEAT",
                    room.guest->name, battle.host.cleared ? 100 : battle.host.bestPercent, battle.guest.bestPercent);
    log::info("Versus debug {}", s.botResult);
}

// 플레이어(호스트) 진행 상황을 봇 방에 반영한다. 시도·최고 퍼센트·클리어 등은 줄어들지 않게 합치고,
// 일시정지 사용량은 예산 안으로 제한한다.
void botReport(BattlePlayerState progress) {
    auto& s = state();
    if (!s.room || !s.room->battle || s.room->battle->finishedAt > 0) return;
    auto& current = s.room->battle->host;
    progress.attemptsUsed = std::max(progress.attemptsUsed, current.attemptsUsed);
    progress.runNumber = std::max(progress.runNumber, current.runNumber);
    progress.bestPercent = std::clamp(std::max(progress.bestPercent, current.bestPercent), 0, 100);
    progress.currentPercent = std::clamp(progress.currentPercent, 0, 100);
    progress.cleared = current.cleared || progress.cleared;
    progress.forfeited = current.forfeited || progress.forfeited;
    progress.userCoins = std::max(progress.userCoins, current.userCoins);
    progress.pauseUsedMs =
        std::clamp<int64_t>(std::max(progress.pauseUsedMs, current.pauseUsedMs), 0, battle::PAUSE_BUDGET_MS);
    progress.updatedAt = nowMs();
    current = std::move(progress);
    botResolve();
}

// 프레임마다 봇을 진행시킨다. 출발 후 3초가 지나면 계획대로 시도를 시작해 시간에 비례해 퍼센트를 올리고,
// 시도가 끝나면 1초 대기 후 다음 시도로 넘어간다.
void botTick(float dt) {
    auto& s = state();
    if (!s.room || !s.room->battle || !s.room->launch || s.room->battle->finishedAt > 0) return;
    auto& room = *s.room;
    auto& bot = room.battle->guest;
    if (room.launch->releasedAt <= 0 || nowMs() < room.launch->releasedAt + 3000) return;
    if (battle::terminal(bot, room.rules) || s.botRun >= s.botRuns.size()) return;
    if (!s.botRunning) {
        s.botWait -= dt;
        if (s.botWait > 0.f) return;
        if (s.botRun > 0) {
            ++bot.runNumber;
            bot.inAttempt = true;
            bot.currentPercent = 0;
        }
        s.botRunning = true;
        s.botTime = 0.f;
        bot.updatedAt = nowMs();
        return;
    }
    auto const [seconds, percent] = s.botRuns[s.botRun];
    s.botTime += dt;
    int const now = std::min(percent, static_cast<int>(percent * s.botTime / std::max(.1f, seconds)));
    if (now != bot.currentPercent) {
        bot.currentPercent = now;
        bot.updatedAt = nowMs();
    }
    if (s.botTime < seconds) return;
    battle::finishAttempt(bot, room.rules, percent);
    bot.updatedAt = nowMs();
    ++s.botRun;
    s.botRunning = false;
    s.botWait = 1.f;
    botResolve();
}
}

bool Service::botBattle() const {
    return state().botBattle;
}

// 봇 배틀을 시작한다. 디버그 모드가 아니거나, 이미 방·그룹·랭크 매치 중이거나, 맵이 없으면 실패.
// 랭크 방처럼 구성한 로컬 방을 만들고 봇의 시도 계획을 준비한다.
bool Service::startBotBattle(LevelInfo level, PlayerProfile bot, std::string& error) {
    auto& s = state();
    if (!debug::active()) {
        error = "Bot battles are for the debug build only.";
        return false;
    }
    if (s.room || s.group) {
        error = "Leave your current room first.";
        return false;
    }
    if (!s.rankedMatch.empty()) {
        error = "A Ranked match is in progress.";
        return false;
    }
    if (level.id <= 0) {
        error = "No map.";
        return false;
    }
    auto const id = "debug-bot-" + randomId();
    RoomInfo room;
    room.id = "ranked-" + id;
    room.name = "Bot battle";
    room.host = s.profile;
    room.guest = std::move(bot);
    room.level = std::move(level);
    room.rules.mode = 0;
    room.rules.attempts = botAttempts(room.level);
    room.rules.practice = false;
    room.started = true;
    room.hostReady = room.guestReady = true;
    room.guestDownload.uid = room.guest->uid;
    room.guestDownload.levelId = room.level.id;
    room.guestDownload.percent = 100;
    room.guestDownload.mapReady = true;
    room.guestDownload.best = kBotBest;
    LaunchInfo launch;
    launch.id = id;
    launch.requestedAt = nowMs();
    launch.guestLoaded = true;
    room.launch = launch;
    BattleInfo battle;
    battle.id = id;
    battle.host = parseBattlePlayer(initialBattlePlayer());
    battle.guest = parseBattlePlayer(initialBattlePlayer());
    room.battle = battle;
    ++s.epoch;
    s.polling = s.writing = false;
    s.room = std::move(room);
    s.rankedMatch = id;
    s.rankedLdm = Mod::get()->getSavedValue<bool>("ranked-ldm", false);
    s.botBattle = true;
    s.botRuns = botPlan(s.room->rules.attempts);
    s.botRun = 0;
    s.botTime = s.botWait = 0.f;
    s.botRunning = false;
    s.botResult.clear();
    s.lastContact = Clock::now();
    log::info("Versus debug: bot battle {} on level {} ({} lives)", id, s.room->level.id, s.room->rules.attempts);
    return true;
}

// 봇 배틀을 정리한다. 끝나기 전에 나가면 그 사실을 결과 메시지로 남긴다.
void Service::endBotBattle() {
    auto& s = state();
    if (!s.botBattle) return;
    if (s.room && s.room->battle && s.room->battle->finishedAt <= 0 && s.botResult.empty())
        s.botResult = "Bot battle left before the end. No rating change, nothing was saved.";
    ++s.epoch;
    s.room.reset();
    s.rankedMatch.clear();
    s.rankedLdm = false;
    s.botBattle = false;
    s.botRuns.clear();
    s.botRun = 0;
    s.botRunning = false;
    s.polling = s.writing = false;
}

// 봇 배틀 결과 메시지를 가져가고 비운다.
std::string Service::takeBotResult() {
    return std::exchange(state().botResult, {});
}
