#pragma once

#include <SFML/Audio/Music.hpp>
#include <filesystem>
#include <vector>

namespace tce {

/**
 * 扫描 assets/music 下的音频（.mp4 / .mp3 / .ogg / .flac / .wav）。
 * 非战斗：一首「地图曲」，在地图/事件/商店等之间**不重复切歌**（已在播则保持）。
 * 战斗：每场从对应曲池**均匀随机**一首（循环播放）；退出战斗时再切回地图曲。
 * 地图曲优先：map_bgm.* → ambient.* / explore.* → 文件名含「圣森」；否则用排序后的第一首。
 * 战斗曲分池：见 assets/music/README.txt（Boss 节点：文件名以 boss_bgm 开头；普通/精英：其余非地图曲目）。
 * 选曲使用独立随机源，不消耗 RunRng，以免影响战斗/奖励随机序列。
 */
class MusicManager {
public:
    void scanAssets();
    void playMapMusic();
    /** @param boss_node true=最终 Boss 节点战，仅从 boss_bgm* 曲池随机；false=普通/精英战 */
    void playRandomBattleMusic(bool boss_node = false);
    /** 停止当前 BGM（如播放带音频的外置 ffplay 前调用）。 */
    void stopMusic();

private:
    enum class ActiveKind { None, Map, Battle };

    bool tryPlayFile(const std::filesystem::path& path, ActiveKind kind);

    sf::Music                                   music_{};
    std::vector<std::filesystem::path>          allPaths_{};
    std::vector<std::filesystem::path>          battlePaths_{};
    std::vector<std::filesystem::path>          normalBattlePaths_{};
    std::vector<std::filesystem::path>          bossBattlePaths_{};
    std::filesystem::path                       mapPath_{};
    ActiveKind                                  activeKind_ = ActiveKind::None;
    std::filesystem::path                       activePath_{};
};

} // namespace tce
