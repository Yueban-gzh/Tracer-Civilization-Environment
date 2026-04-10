#include "Common/MusicManager.hpp"
#include "Common/UserSettings.hpp"

#include <SFML/Audio/Listener.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <random>

namespace tce {
namespace {

namespace fs = std::filesystem;

bool ext_supported(const fs::path& p) {
    std::string e = p.extension().u8string();
    for (char& c : e)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e == ".mp4" || e == ".mp3" || e == ".ogg" || e == ".flac" || e == ".wav";
}

bool stem_equals_ascii_ci(const fs::path& p, const char* asciiStem) {
    const std::string s = p.stem().u8string();
    const size_t      n = std::strlen(asciiStem);
    if (s.size() != n) return false;
    for (size_t i = 0; i < n; ++i) {
        unsigned char a = static_cast<unsigned char>(s[i]);
        unsigned char b = static_cast<unsigned char>(asciiStem[i]);
        if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

// UTF-8「圣森」
bool stem_contains_map_hint(const fs::path& p) {
    static const unsigned char kHint[] = {0xE5, 0x9C, 0xA3, 0xE6, 0xA3, 0xAE, 0};
    const std::string          stem    = p.stem().u8string();
    return stem.find(reinterpret_cast<const char*>(kHint)) != std::string::npos;
}

fs::path pick_map_track(const std::vector<fs::path>& sorted) {
    static const char* kAsciiStems[] = {"map_bgm", "ambient", "explore"};
    for (const char* stem : kAsciiStems) {
        for (const fs::path& p : sorted) {
            if (stem_equals_ascii_ci(p, stem)) return p;
        }
    }
    for (const fs::path& p : sorted) {
        if (stem_contains_map_hint(p)) return p;
    }
    return {};
}

void build_battle_list(const std::vector<fs::path>& all, const fs::path& mapP, std::vector<fs::path>& outBattle) {
    outBattle.clear();
    outBattle.reserve(all.size());
    for (const fs::path& p : all) {
        if (!mapP.empty() && p == mapP) continue;
        outBattle.push_back(p);
    }
    if (outBattle.empty()) outBattle = all;
}

} // namespace

void MusicManager::scanAssets() {
    allPaths_.clear();
    battlePaths_.clear();
    mapPath_.clear();

    const fs::path dir = fs::u8path("assets/music");
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        music_.stop();
        activeKind_ = ActiveKind::None;
        activePath_.clear();
        return;
    }

    for (const fs::directory_entry& ent : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!ent.is_regular_file(ec)) continue;
        const fs::path p = ent.path();
        if (!ext_supported(p)) continue;
        allPaths_.push_back(p);
    }
    std::sort(allPaths_.begin(), allPaths_.end(),
              [](const fs::path& a, const fs::path& b) { return a.u8string() < b.u8string(); });

    mapPath_ = pick_map_track(allPaths_);
    build_battle_list(allPaths_, mapPath_, battlePaths_);

    if (allPaths_.empty()) {
        music_.stop();
        activeKind_ = ActiveKind::None;
        activePath_.clear();
    }
}

bool MusicManager::tryPlayFile(const fs::path& path, ActiveKind kind) {
    if (path.empty()) return false;
    if (!music_.openFromFile(path)) return false;
    music_.setLooping(true);
    music_.setVolume(100.f);
    sf::Listener::setGlobalVolume(UserSettings::instance().masterVolume());
    music_.play();
    activeKind_ = kind;
    activePath_ = path;
    return true;
}

void MusicManager::playMapMusic() {
    const bool mapExplicit = !mapPath_.empty();
    if (activeKind_ == ActiveKind::Map && music_.getStatus() == sf::SoundSource::Status::Playing) {
        if (mapExplicit) {
            if (activePath_ == mapPath_) return;
        } else {
            // 未指定 map_bgm 等时：已在播非战斗曲则保持，不在非战斗流程里反复 open 同一逻辑
            return;
        }
    }

    if (!mapPath_.empty()) {
        if (tryPlayFile(mapPath_, ActiveKind::Map)) return;
    }
    for (const fs::path& p : allPaths_) {
        if (tryPlayFile(p, ActiveKind::Map)) return;
    }
    music_.stop();
    activeKind_ = ActiveKind::None;
    activePath_.clear();
}

void MusicManager::playRandomBattleMusic() {
    const auto& pool = battlePaths_;
    if (pool.empty()) {
        playMapMusic();
        return;
    }
    static std::mt19937 pickGen{[] {
        std::random_device rd;
        return std::mt19937(rd());
    }()};
    const int n = static_cast<int>(pool.size());
    std::uniform_int_distribution<int> dist(0, n - 1);
    const int start = dist(pickGen);
    for (int k = 0; k < n; ++k) {
        const int i = (start + k) % n;
        if (tryPlayFile(pool[static_cast<size_t>(i)], ActiveKind::Battle)) return;
    }
    playMapMusic();
}

} // namespace tce
