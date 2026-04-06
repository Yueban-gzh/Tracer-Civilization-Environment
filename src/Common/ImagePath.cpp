#include "Common/ImagePath.hpp"

#include <filesystem>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

namespace fs = std::filesystem;

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
    if (dir.empty()) return rel;
    if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) return rel;
    const char last = dir.back();
    if (last == '/' || last == '\\') return dir + rel;
    return dir + "/" + rel;
}

static const char* const kImageExts[] = {".png",  ".jpg",  ".jpeg", ".PNG", ".JPG", ".JPEG",
                                         ".jfif", ".JFIF"};

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
    for (int i = 0; i < 10; ++i) {
        if (start.empty()) break;
        if (fs::exists(start / "assets", ec) && fs::exists(start / "data", ec)) {
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

    auto push_prefix = [&](const std::string& prefix) {
        for (const char* ext : kImageExts) {
            candidates.push_back(prefix + base_no_ext + ext);
        }
    };
    push_prefix("");
    push_prefix("./");
    push_prefix("../");
    push_prefix("../../");
    push_prefix("../../../");
    push_prefix("../../../../");

#ifdef _WIN32
    const std::string exeDir = get_executable_directory();
    if (!exeDir.empty()) {
        for (const char* up : {"", "../", "../../", "../../../", "../../../../", "../../../../../"}) {
            for (const char* ext : kImageExts) {
                candidates.push_back(join_path(exeDir, std::string(up) + base_no_ext + ext));
            }
        }
    }
#endif

    for (const auto& p : candidates) {
        if (file_exists_utf8(p)) return p;
    }
    return {};
}

} // namespace tce
