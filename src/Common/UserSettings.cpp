#include "Common/UserSettings.hpp"

#include <SFML/Audio/Listener.hpp>
#include <fstream>
#include <filesystem>

#include "DataLayer/JsonParser.h"

namespace tce {
namespace {

namespace fs = std::filesystem;

struct VideoModeEntry {
    unsigned    w = 0;
    unsigned    h = 0;
    bool        fullscreen = false;
};

const VideoModeEntry kVideoModes[] = {
    {1280, 720, false},
    {1600, 900, false},
    {1920, 1080, false},
    {0, 0, true}, // 全屏，使用桌面分辨率
};

static const char* kSettingsPath = "saves/user_settings.json";

static void ensure_saves_dir() {
    std::error_code ec;
    fs::create_directories(fs::u8path("saves"), ec);
}

} // namespace

UserSettings& UserSettings::instance() {
    static UserSettings s;
    return s;
}

void UserSettings::load() {
    std::error_code ec;
    if (!fs::exists(fs::u8path(kSettingsPath), ec)) return;
    try {
        DataLayer::JsonValue root = DataLayer::parse_json_file(kSettingsPath);
        if (!root.is_object()) return;
        if (const auto* v = root.get_key("master_volume")) {
            if (v->is_int()) master_volume_ = static_cast<float>(v->i);
            // 兼容浮点存成 string 的情况略过
        }
        if (const auto* v = root.get_key("video_preset")) {
            if (v->is_int()) {
                video_preset_ = v->i;
                if (video_preset_ < 0 || video_preset_ >= kVideoModeCount) video_preset_ = 2;
            }
        }
    } catch (...) {
    }
    if (master_volume_ < 0.f) master_volume_ = 0.f;
    if (master_volume_ > 100.f) master_volume_ = 100.f;
    applyAudio();
}

void UserSettings::save() {
    ensure_saves_dir();
    std::ofstream out(fs::u8path(kSettingsPath).string(), std::ios::binary);
    if (!out) return;
    out << "{\n";
    out << "  \"master_volume\": " << static_cast<int>(master_volume_ + 0.5f) << ",\n";
    out << "  \"video_preset\": " << video_preset_ << "\n";
    out << "}\n";
}

void UserSettings::setMasterVolume(float percent0to100) {
    if (percent0to100 < 0.f) percent0to100 = 0.f;
    if (percent0to100 > 100.f) percent0to100 = 100.f;
    master_volume_ = percent0to100;
    applyAudio();
    save();
}

void UserSettings::setVideoPreset(int index) {
    if (index < 0) index = 0;
    if (index >= kVideoModeCount) index = kVideoModeCount - 1;
    video_preset_ = index;
}

std::wstring UserSettings::videoPresetLabelForIndex(int index) const {
    if (index < 0) index = 0;
    if (index >= kVideoModeCount) index = kVideoModeCount - 1;
    const VideoModeEntry& m = kVideoModes[static_cast<size_t>(index)];
    if (m.fullscreen) return L"全屏（桌面分辨率）";
    return std::to_wstring(m.w) + L" × " + std::to_wstring(m.h) + L"（窗口）";
}

std::wstring UserSettings::videoPresetLabel() const {
    return videoPresetLabelForIndex(video_preset_);
}

void UserSettings::applyAudio() const {
    sf::Listener::setGlobalVolume(master_volume_);
}

void UserSettings::applyVideoModeToWindow(sf::RenderWindow& window, const sf::ContextSettings& ctx) const {
    const VideoModeEntry& m = kVideoModes[static_cast<size_t>(video_preset_)];
    if (m.fullscreen) {
        const sf::VideoMode dm = sf::VideoMode::getDesktopMode();
        window.create(dm, "Tracer Civilization", sf::State::Fullscreen, ctx);
    } else {
        window.create(sf::VideoMode({m.w, m.h}),
                      "Tracer Civilization",
                      sf::Style::Close | sf::Style::Titlebar,
                      sf::State::Windowed,
                      ctx);
    }
    window.setFramerateLimit(60);
}

} // namespace tce
