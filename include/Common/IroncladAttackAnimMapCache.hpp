#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <vector>

namespace tce {

/**
 * OpenGL 上下文失效或路径列表失效时清空地图侧预载与跨战斗会话缓存
 * （与 BattleUI::clear_gpu_cached_textures 配对调用）。
 */
void ironclad_attack_anim_map_cache_invalidate();

/**
 * 地图界面在「无鼠标左右键按下」时空闲推进：每帧最多上传 max_textures_per_idle_frame 张纹理。
 * 仅当前角色为 Ironclad 时填充缓存；否则释放缓存以省显存。
 */
void ironclad_attack_anim_map_cache_tick(sf::RenderWindow& window,
                                         const std::string& character_id,
                                         int max_textures_per_idle_frame = 2);

/**
 * 在 warm 已扫描 pending 路径后调用：若缓存路径与 pending 完全一致，则移入已解码纹理并清空地图缓存。
 * @return true 表示 battle_load_index 已达序列长度，可跳过 BattleUI::preload_ironclad_attack_anim_blocking。
 */
bool ironclad_attack_anim_map_cache_try_adopt(const std::vector<std::string>& battle_pending_paths,
                                              std::vector<sf::Texture>& battle_frames,
                                              size_t& battle_load_index);

/**
 * 上一场战斗已完整解码并提交时，本场直接复用会话内 GPU 纹理，避免连续战斗间隔短时重复解码。
 * @param battle_pending_paths 须与 warm 扫描结果一致
 * @return true 且 battle_pending_paths 非空时表示已借用到会话纹理；空路径时清空 out 并返回 true
 */
bool ironclad_attack_anim_session_try_borrow(const std::vector<std::string>& battle_pending_paths,
                                             std::shared_ptr<std::vector<sf::Texture>>& out_shared,
                                             size_t& battle_load_index);

/** 在磁盘序列已全部处理完毕后调用：将本场 vector 移入会话并令 battle 与 out_shared 指向同一块存储。 */
void ironclad_attack_anim_session_commit(const std::vector<std::string>& pending_paths,
                                         std::vector<sf::Texture>&& battle_frames,
                                         std::shared_ptr<std::vector<sf::Texture>>& battle_shared_out);

} // namespace tce
