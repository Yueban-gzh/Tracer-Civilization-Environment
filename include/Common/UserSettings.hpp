#pragma once

#include <SFML/Graphics.hpp>
#include <string>

namespace tce {

/**
 * 用户偏好：主音量、分辨率预设。持久化 saves/user_settings.json。
 * 分辨率需在持有 ContextSettings 的代码里调用 applyVideoModeToWindow / pollVideoApply。
 */
class UserSettings {
public:
    static UserSettings& instance();

    void load();
    void save();

    float masterVolume() const { return master_volume_; }
    /** 立即生效并写入存档（音量无需单独「应用」） */
    void setMasterVolume(float percent0to100);

    /** 当前视频预设下标 0..kVideoModeCount-1 */
    int videoPreset() const { return video_preset_; }
    void setVideoPreset(int index);

    std::wstring videoPresetLabel() const;
    std::wstring videoPresetLabelForIndex(int index) const;

    /** 用户点击「应用分辨率」后置位；由主循环 / GameFlow::run 消费并重建窗口 */
    void markVideoApplyPending() { video_apply_pending_ = true; }
    bool consumeVideoApplyPending() {
        const bool t = video_apply_pending_;
        video_apply_pending_ = false;
        return t;
    }

    void applyAudio() const;

    /** 按当前预设重建窗口（须与 main 中 ContextSettings 一致） */
    void applyVideoModeToWindow(sf::RenderWindow& window, const sf::ContextSettings& ctx) const;

    static constexpr int kVideoModeCount = 4;

private:
    UserSettings() = default;

    float master_volume_        = 70.f;
    int   video_preset_         = 2; // 默认 1920×1080 窗口
    bool  video_apply_pending_  = false;
};

} // namespace tce
