#include "Common/IroncladAttackAnimMapCache.hpp"

#include "Common/ImagePath.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <utility>

namespace tce {

namespace {

constexpr unsigned kIroncladAttackAnimMaxTextureSidePx = 1024u;

struct MapGpuCache {
    std::vector<std::string> paths;
    /** 已从 paths 尝试加载到下标（与 BattleUI::ironcladAttackAnimLoadIndex_ 同语义，含解码失败仍前进）。 */
    size_t next_path_index = 0;
    std::vector<sf::Texture> frames;
};

MapGpuCache g_ironclad_attack_anim_map_gpu;

std::vector<std::string> g_ironclad_attack_anim_session_paths;
std::shared_ptr<std::vector<sf::Texture>> g_ironclad_attack_anim_session_frames;

/** 与 BattleUI::load_sf_texture_utf8_max_side 同规则，供地图侧独立缓存解码路径一致。 */
bool load_sf_texture_utf8_max_side(sf::Texture& tex, const std::string& utf8Path, unsigned maxSide) {
    if (utf8Path.empty() || maxSide < 32u)
        return false;
    namespace fs = std::filesystem;
    fs::path p = fs::u8path(utf8Path);
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) {
        if (p.is_absolute())
            return false;
        const fs::path cwd = fs::current_path(ec);
        if (ec)
            return false;
        p = cwd / p;
        if (!fs::is_regular_file(p, ec))
            return false;
    }
    sf::Image img;
    if (!img.loadFromFile(p))
        return false;
    const sf::Vector2u sz = img.getSize();
    const unsigned w = sz.x;
    const unsigned h = sz.y;
    if (w == 0 || h == 0)
        return false;
    const unsigned longSide = std::max(w, h);
    if (longSide <= maxSide)
        return tex.loadFromImage(img);
    const double scale = static_cast<double>(maxSide) / static_cast<double>(longSide);
    const unsigned nw = std::max(1u, static_cast<unsigned>(std::lround(static_cast<double>(w) * scale)));
    const unsigned nh = std::max(1u, static_cast<unsigned>(std::lround(static_cast<double>(h) * scale)));
    sf::Image small(sf::Vector2u(nw, nh), sf::Color::Transparent);
    for (unsigned y = 0; y < nh; ++y) {
        for (unsigned x = 0; x < nw; ++x) {
            const unsigned sx = std::min(w - 1u, (x * w) / nw);
            const unsigned sy = std::min(h - 1u, (y * h) / nh);
            small.setPixel(sf::Vector2u(x, y), img.getPixel(sf::Vector2u(sx, sy)));
        }
    }
    return tex.loadFromImage(small);
}

} // namespace

void ironclad_attack_anim_map_cache_invalidate() {
    g_ironclad_attack_anim_map_gpu = {};
    g_ironclad_attack_anim_session_paths.clear();
    g_ironclad_attack_anim_session_frames.reset();
}

void ironclad_attack_anim_map_cache_tick(sf::RenderWindow& window, const std::string& character_id, int max_textures_per_idle_frame) {
    if (character_id != "Ironclad") {
        if (!g_ironclad_attack_anim_map_gpu.paths.empty() || !g_ironclad_attack_anim_map_gpu.frames.empty() ||
            g_ironclad_attack_anim_map_gpu.next_path_index != 0)
            ironclad_attack_anim_map_cache_invalidate();
        return;
    }

    if (max_textures_per_idle_frame <= 0)
        return;

    if (g_ironclad_attack_anim_map_gpu.paths.empty()) {
        scan_ironclad_attack_anim_paths(g_ironclad_attack_anim_map_gpu.paths);
        g_ironclad_attack_anim_map_gpu.frames.clear();
        g_ironclad_attack_anim_map_gpu.next_path_index = 0;
    }

    if (g_ironclad_attack_anim_map_gpu.paths.empty())
        return;

    if (g_ironclad_attack_anim_session_frames && !g_ironclad_attack_anim_session_paths.empty() &&
        g_ironclad_attack_anim_session_paths == g_ironclad_attack_anim_map_gpu.paths)
        return;

    const size_t n = g_ironclad_attack_anim_map_gpu.paths.size();
    if (g_ironclad_attack_anim_map_gpu.next_path_index >= n)
        return;

    (void)window.setActive(true);

    int budget = max_textures_per_idle_frame;
    while (budget-- > 0 && g_ironclad_attack_anim_map_gpu.next_path_index < n) {
        const size_t i = g_ironclad_attack_anim_map_gpu.next_path_index;
        const std::string& pth = g_ironclad_attack_anim_map_gpu.paths[i];
        try {
            sf::Texture t;
            if (load_sf_texture_utf8_max_side(t, pth, kIroncladAttackAnimMaxTextureSidePx)) {
                t.setSmooth(true);
                g_ironclad_attack_anim_map_gpu.frames.push_back(std::move(t));
            }
        } catch (const std::exception& e) {
            std::cerr << "[IroncladAnimMapCache] tick: " << pth << " : " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[IroncladAnimMapCache] tick: " << pth << " : unknown exception\n";
        }
        ++g_ironclad_attack_anim_map_gpu.next_path_index;
    }
}

bool ironclad_attack_anim_map_cache_try_adopt(const std::vector<std::string>& battle_pending_paths,
                                              std::vector<sf::Texture>& battle_frames,
                                              size_t& battle_load_index) {
    if (battle_pending_paths.empty()) {
        ironclad_attack_anim_map_cache_invalidate();
        battle_frames.clear();
        battle_load_index = 0;
        return true;
    }

    if (g_ironclad_attack_anim_map_gpu.paths.empty())
        return false;

    if (g_ironclad_attack_anim_map_gpu.paths != battle_pending_paths) {
        ironclad_attack_anim_map_cache_invalidate();
        return false;
    }

    if (g_ironclad_attack_anim_map_gpu.next_path_index > g_ironclad_attack_anim_map_gpu.paths.size()) {
        ironclad_attack_anim_map_cache_invalidate();
        return false;
    }

    battle_frames = std::move(g_ironclad_attack_anim_map_gpu.frames);
    battle_load_index = g_ironclad_attack_anim_map_gpu.next_path_index;
    g_ironclad_attack_anim_map_gpu.paths.clear();
    g_ironclad_attack_anim_map_gpu.frames.clear();
    g_ironclad_attack_anim_map_gpu.next_path_index = 0;
    return battle_load_index >= battle_pending_paths.size();
}

bool ironclad_attack_anim_session_try_borrow(const std::vector<std::string>& battle_pending_paths,
                                             std::shared_ptr<std::vector<sf::Texture>>& out_shared,
                                             size_t& battle_load_index) {
    if (battle_pending_paths.empty()) {
        out_shared.reset();
        battle_load_index = 0;
        return true;
    }
    if (!g_ironclad_attack_anim_session_frames || g_ironclad_attack_anim_session_paths != battle_pending_paths)
        return false;
    out_shared = g_ironclad_attack_anim_session_frames;
    battle_load_index = battle_pending_paths.size();
    return true;
}

void ironclad_attack_anim_session_commit(const std::vector<std::string>& pending_paths,
                                         std::vector<sf::Texture>&& battle_frames,
                                         std::shared_ptr<std::vector<sf::Texture>>& battle_shared_out) {
    if (pending_paths.empty())
        return;
    auto sp = std::make_shared<std::vector<sf::Texture>>(std::move(battle_frames));
    g_ironclad_attack_anim_session_paths = pending_paths;
    g_ironclad_attack_anim_session_frames = sp;
    battle_shared_out = g_ironclad_attack_anim_session_frames;
}

} // namespace tce
