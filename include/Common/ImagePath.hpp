#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace tce {

/**
 * 在程序启动时尽早调用：从 exe 所在目录向上查找同时包含 assets/ 与 data/ 的目录，
 * 并设为当前工作目录，使相对路径 assets/...、data/... 与 DataLayer 的 JSON 加载一致。
 */
void setup_asset_working_directory();

/** 可执行文件所在目录（UTF-8）。Windows 下用宽字符 API，避免中文路径被 ANSI 截断。 */
std::string get_executable_directory_utf8();

/** 判断 UTF-8 路径是否存在。 */
bool file_exists_utf8(const std::string& p);

/**
 * 解析无扩展名基路径（如 assets/monsters/foo 或 assets/relics/burning_blood）：
 * 依次尝试 .png / .jpg / .jpeg / .jfif（及常见大写），并在多种 cwd、exe 相对路径下查找。
 */
std::string resolve_image_path(const std::string& base_no_ext);

/**
 * 追加待扫描的 assets/status 目录（当前工作目录与 exe 目录及若干上级），
 * 与 resolve_image_path 的搜寻范围一致，避免仅依赖 cwd 时索引为空。
 */
void append_status_effect_icon_scan_roots(std::vector<std::filesystem::path>& out);

/**
 * 铁甲战士攻击序列帧目录：与状态图标相同，从 exe 上溯工程根、cwd、相对路径等收集候选，
 * 避免仅依赖 cwd 时从 build/ 启动找不到 assets/animations/Ironclad_attack。
 */
void append_ironclad_attack_anim_dir_candidates(std::vector<std::filesystem::path>& out);

/**
 * 扫描首个可用的 Ironclad_attack 目录下 PNG（排序后 UTF-8 路径），与 BattleUI 进战斗预载规则一致。
 */
void scan_ironclad_attack_anim_paths(std::vector<std::string>& out_utf8_paths);

} // namespace tce
