#include "Common/ImagePath.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

namespace fs = std::filesystem;

/** 统一为本机首选分隔符（Windows 为 \\），避免 D:\\...\\assets/status 混用 / 与 \\。 */
static std::string path_to_consistent_utf8(const fs::path& p) {
    if (p.empty())
        return {};
    fs::path n = p.lexically_normal();
    n.make_preferred();
    return n.u8string();
}

std::string get_executable_directory() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    std::string full(buf, buf + n);
    const size_t pos = full.find_last_of("\\/");
    if (pos == std::string::npos) return {};
    return full.substr(0, pos);
#else
    return {};
#endif
}

static std::string join_path(const std::string& dir, const std::string& rel) {
    if (rel.empty())
        return path_to_consistent_utf8(fs::u8path(dir));
    if (!dir.empty() && (rel[0] == '/' || rel[0] == '\\'))
        return path_to_consistent_utf8(fs::u8path(rel));
    if (dir.empty())
        return path_to_consistent_utf8(fs::u8path(rel));
    return path_to_consistent_utf8(fs::u8path(dir) / fs::u8path(rel));
}

static const char* const kImageExts[] = {".png",  ".jpg",  ".jpeg", ".PNG", ".JPG", ".JPEG",
                                         ".jfif", ".JFIF"};

/** 路径最后一级目录名转小写（仅 ASCII），用于识别 VS/MSBuild 输出目录。 */
static std::string path_leaf_lower_ascii(const fs::path& dir) {
    const fs::path leaf = dir.filename();
    if (leaf.empty())
        return {};
    std::string s = leaf.generic_string();
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

/** Debug/Release/x64 等目录下常有误拷的 assets+data，不能当作工程根。 */
static bool is_typical_build_output_folder(const fs::path& dir) {
    const std::string leaf = path_leaf_lower_ascii(dir);
    if (leaf.empty())
        return false;
    static constexpr std::string_view kBad[] = {
        "debug",         "release",      "relwithdebinfo", "minsizerel",
        "x64",           "x86",          "win32",          "x32",
        "arm64",         "arm",          "profile",        "distribution",
        "tracing",       "coverage",
    };
    for (const std::string_view b : kBad) {
        if (leaf == b)
            return true;
    }
    return false;
}

/**
 * 从 start 向上找：同时含 assets/ 与 data/，且最后一级目录名不是 Debug/Release/x64 等输出目录。
 * 避免 x64/Debug 里整套误拷资源被当成工程根。
 */
static fs::path find_first_ancestor_with_game_content_root(const fs::path& start) {
    fs::path step = start;
    for (int i = 0; i < 24; ++i) {
        std::error_code ec;
        if (!is_typical_build_output_folder(step)
            && fs::exists(step / "assets", ec) && fs::exists(step / "data", ec))
            return step;
        fs::path parent = step.parent_path();
        if (parent == step)
            break;
        step = std::move(parent);
    }
    return {};
}

} // namespace

namespace tce {

void setup_asset_working_directory() {
    std::error_code ec;
#ifdef _WIN32
    std::string exeDir = get_executable_directory();
    if (exeDir.empty()) return;
    fs::path start = fs::u8path(exeDir);
#else
    fs::path start = fs::current_path(ec);
#endif
    for (int i = 0; i < 24; ++i) {
        if (start.empty()) break;
        if (!is_typical_build_output_folder(start)
            && fs::exists(start / "assets", ec) && fs::exists(start / "data", ec)) {
            fs::current_path(start, ec);
            return;
        }
        fs::path parent = start.parent_path();
        if (parent == start) break;
        start = std::move(parent);
    }
}

bool file_exists_utf8(const std::string& p) {
    std::error_code ec;
    return fs::exists(fs::u8path(p), ec);
}

std::string resolve_image_path(const std::string& base_no_ext) {
    std::vector<std::string> candidates;

    // 先按「exe 向上 → 同时含 assets/ 与 data/ 的工程根」解析，勿停在 x64/Debug 下的假 assets
#ifdef _WIN32
    if (const std::string exeDir = get_executable_directory(); !exeDir.empty()) {
        const fs::path root = find_first_ancestor_with_game_content_root(fs::u8path(exeDir));
        if (!root.empty()) {
            for (const char* ext : kImageExts) {
                const fs::path file = root / fs::u8path(base_no_ext + ext);
                std::error_code ec;
                if (fs::is_regular_file(file, ec) && !ec)
                    return path_to_consistent_utf8(file);
            }
        }
    }
#endif

    auto push_prefix = [&](const std::string& prefix) {
        for (const char* ext : kImageExts) {
            candidates.push_back(prefix + base_no_ext + ext);
        }
    };
    // cwd 在 x64/Debug 时，"" / "./" 会命中输出目录下的假 assets，对 assets/ 路径跳过
    std::error_code ecwd;
    const fs::path cwdForResolve = fs::current_path(ecwd);
    const bool skip_dot_assets = !ecwd && base_no_ext.size() >= 7u
        && base_no_ext.compare(0, 7, "assets/") == 0
        && is_typical_build_output_folder(cwdForResolve);
    if (!skip_dot_assets) {
        push_prefix("");
        push_prefix("./");
    }
    push_prefix("../");
    push_prefix("../../");
    push_prefix("../../../");
    push_prefix("../../../../");

#ifdef _WIN32
    const std::string exeDir = get_executable_directory();
    if (!exeDir.empty()) {
        for (const char* up : {"", "../", "../../", "../../../", "../../../../", "../../../../../"}) {
            // 禁止 exeDir + "assets/..."（即 x64/Debug/assets），避免命中输出目录里的空壳/损坏图
            if (up[0] == '\0' && base_no_ext.size() >= 7u && base_no_ext.compare(0, 7, "assets/") == 0)
                continue;
            for (const char* ext : kImageExts) {
                candidates.push_back(join_path(exeDir, std::string(up) + base_no_ext + ext));
            }
        }
    }
#endif

    for (const auto& p : candidates) {
        if (file_exists_utf8(p))
            return path_to_consistent_utf8(fs::u8path(p));
    }
    return {};
}

void append_status_effect_icon_scan_roots(std::vector<std::filesystem::path>& out) {
    std::unordered_set<std::string> seen;
    auto try_push = [&](const fs::path& statusDir) {
        std::error_code ec;
        if (!fs::is_directory(statusDir, ec) || ec)
            return;
        fs::path canon = fs::weakly_canonical(statusDir, ec);
        if (ec)
            canon = fs::absolute(statusDir, ec);
        const std::string key = canon.u8string();
        if (!seen.insert(key).second)
            return;
        out.push_back(statusDir);
    };
    auto push_from_start_dir = [&](const fs::path& start) {
        const fs::path root = find_first_ancestor_with_game_content_root(start);
        if (!root.empty())
            try_push(root / "assets" / "status");
    };
#ifdef _WIN32
    if (const std::string exeDir = get_executable_directory(); !exeDir.empty())
        push_from_start_dir(fs::u8path(exeDir));
#endif
    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    if (!ec && !cwd.empty())
        push_from_start_dir(std::move(cwd));
}

} // namespace tce
