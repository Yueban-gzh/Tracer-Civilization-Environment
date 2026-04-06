#pragma once

#include <string>

namespace tce {

/**
 * 在程序启动时尽早调用：从 exe 所在目录向上查找同时包含 assets/ 与 data/ 的目录，
 * 并设为当前工作目录，使相对路径 assets/...、data/... 与 DataLayer 的 JSON 加载一致。
 */
void setup_asset_working_directory();

/** 判断 UTF-8 路径是否存在。 */
bool file_exists_utf8(const std::string& p);

/**
 * 解析无扩展名基路径（如 assets/monsters/foo 或 assets/relics/burning_blood）：
 * 依次尝试 .png / .jpg / .jpeg / .jfif（及常见大写），并在多种 cwd、exe 相对路径下查找。
 */
std::string resolve_image_path(const std::string& base_no_ext);

} // namespace tce
