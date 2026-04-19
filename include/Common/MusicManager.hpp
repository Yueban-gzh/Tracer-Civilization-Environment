#pragma once

#include <SFML/Audio/Music.hpp>
#include <filesystem>
#include <vector>

namespace tce {

/**
 * 扫描 assets/music 下的音频（.mp4 / .mp3 / .ogg / .flac / .wav）。
 * 非战斗：一首「地图曲」，在地图/事件/商店等之间**不重复切歌**（已在播则保持）。
 * 战斗：从其余曲目中每场随机一首（循环播放）；退出战斗时再切回地图曲。
 * 地图曲优先：map_bgm.* → ambient.* / explore.* → 文件名含「圣森」；否则用排序后的第一首。
 * 选曲使用独立随机源，不消耗 RunRng，以免影响战斗/奖励随机序列。
 */
class MusicManager {
public:
    void scanAssets();
    void playMapMusic();
    void playRandomBattleMusic();
    /** 停止当前 BGM（如播放带音频的外置 ffplay 前调用）。 */
    void stopMusic();

private:
    enum class ActiveKind { None, Map, Battle };

    bool tryPlayFile(const std::filesystem::path& path, ActiveKind kind);

    sf::Music                                   music_{};
    std::vector<std::filesystem::path>          allPaths_{};
    std::vector<std::filesystem::path>          battlePaths_{};
    std::filesystem::path                       mapPath_{};
    ActiveKind                                  activeKind_ = ActiveKind::None;
    std::filesystem::path                       activePath_{};
};

} // namespace tce
