#include "GameFlow/GameFlowController.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "Common/ImagePath.hpp"

namespace tce {

    namespace {

    float clamp01(float v) {
        return std::max(0.0f, std::min(1.0f, v));
    }

    struct StorySceneLine {
        std::wstring text;
    };

    /** 先秦 / 汉唐 / 宋明：优先使用 introduce 下章节背景，失败则回退到旧资源。 */
    std::string resolveCinematicBackgroundPath(int mapIdx, const std::string& preferredPath) {
        if (!preferredPath.empty()) {
            const std::string preferred = resolve_image_path(preferredPath);
            if (!preferred.empty()) return preferred;
        }

        static const char* const kIntroBgs[] = {
            "assets/introduce/bg_qin",
            "assets/introduce/bg_tang",
            "assets/introduce/bg_song",
        };

        static const char* const kLegacyBgs[] = {
            "assets/backgrounds/bg_xianqin",
            "assets/backgrounds/bg_hantang",
            "assets/backgrounds/bg_songming",
        };

        if (mapIdx < 0) mapIdx = 0;
        if (mapIdx > 2) mapIdx = 2;

        const std::string introPath = resolve_image_path(kIntroBgs[mapIdx]);
        if (!introPath.empty()) return introPath;

        const std::string legacyPath = resolve_image_path(kLegacyBgs[mapIdx]);
        if (!legacyPath.empty()) return legacyPath;

        const std::string dialogBg = resolve_image_path("assets/backgrounds/dialog_bg");
        if (!dialogBg.empty()) return dialogBg;
        return "assets/backgrounds/dialog_bg.png";
    }

    void layoutCoverSprite(sf::Sprite& sprite, const sf::Texture& tex, const sf::Vector2f& winSize, float t) {
        const sf::Vector2u texSize = tex.getSize();
        if (texSize.x == 0 || texSize.y == 0) return;
        float scale = std::max(
            winSize.x / static_cast<float>(texSize.x),
            winSize.y / static_cast<float>(texSize.y)
        );
        scale *= 1.02f + 0.015f * std::sin(t * 0.45f);
        sprite.setScale(sf::Vector2f(scale, scale));
        const sf::FloatRect gb = sprite.getGlobalBounds();
        const float ox = (winSize.x - gb.size.x) * 0.5f + std::sin(t * 0.35f) * 12.0f;
        const float oy = (winSize.y - gb.size.y) * 0.5f - 16.0f;
        sprite.setPosition(sf::Vector2f(ox, oy));
    }

    void runStoryDialogAnimationImpl(
        sf::RenderWindow& window,
        GameFlowController* ironclad_map_preload_host,
        const sf::Font& hudFont,
        bool hudFontLoaded,
        const std::vector<StorySceneLine>& lines,
        const std::string& backgroundPath
    ) {
        if (lines.empty()) return;

        constexpr float kLineDuration = 4.4f;
        constexpr float kCrossFadeDuration = 0.38f;
        constexpr float kSceneInDuration = 0.8f;
        constexpr float kSceneOutDuration = 0.95f;
        const float totalDuration = static_cast<float>(lines.size()) * kLineDuration;

        sf::Clock clock;
        bool animationComplete = false;
        int ironcladPreloadStoryFrames = 0;

        sf::Texture backgroundTexture;
        std::unique_ptr<sf::Sprite> backgroundSprite;
        bool backgroundLoaded = false;

        if (backgroundTexture.loadFromFile(backgroundPath)) {
            backgroundLoaded = true;
            backgroundSprite = std::make_unique<sf::Sprite>(backgroundTexture);
        }

        while (window.isOpen() && !animationComplete) {
            const float t = clock.getElapsedTime().asSeconds();
            if (t >= totalDuration) animationComplete = true;

            while (const std::optional event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) {
                    window.close();
                    return;
                }
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
                        animationComplete = true;
                    }
                }
            }

            const sf::Vector2f winSize = sf::Vector2f(window.getSize());
            window.clear(sf::Color(8, 7, 12));

            if (backgroundLoaded && backgroundSprite) {
                layoutCoverSprite(*backgroundSprite, backgroundTexture, winSize, t);
                window.draw(*backgroundSprite);
            }

            // ========== 剧情字幕布局（结合运行效果只改这里即可）==========
            // 文件：src/GameFlow/BattleEntryAnimation.cpp，函数：runStoryDialogAnimationImpl
            //
            // kMarginXFrac     左边距占窗口宽度比例（0~1）
            // kBodyFromBottom  正文第一行距窗口底边的像素（增大则整体上移）
            // kSpeakerGap      「【智者】」标题相对正文向上偏移的像素
            // kHintFromRight   「按 ESC 跳过」距右边距的像素（与 marginX 配合）
            // kHintFromBottom  提示文字距窗口底边的像素
            constexpr float kMarginXFrac = 0.15f;
            constexpr float kBodyFromBottom = 200.0f;
            constexpr float kSpeakerGap = 44.0f;
            constexpr float kHintFromRight = 280.0f;
            constexpr float kHintFromBottom = 28.0f;
            // ==============================================================

            const float marginX = winSize.x * kMarginXFrac;
            const float bodyY = winSize.y - kBodyFromBottom;
            const float speakerY = bodyY - kSpeakerGap;

            if (hudFontLoaded) {
                sf::Text speaker(hudFont, L"【智者】", 22);
                speaker.setPosition(sf::Vector2f(marginX, speakerY));
                speaker.setFillColor(sf::Color(115, 78, 38, 250));
                window.draw(speaker);

                const int currentLine = std::min(
                    static_cast<int>(lines.size()) - 1,
                    std::max(0, static_cast<int>(t / kLineDuration))
                );
                const float localT = std::fmod(t, kLineDuration);
                const bool canBlendNext = (currentLine + 1) < static_cast<int>(lines.size());
                const float blendToNext = canBlendNext
                    ? clamp01((localT - (kLineDuration - kCrossFadeDuration)) / kCrossFadeDuration)
                    : 0.0f;

                const unsigned textSize = static_cast<unsigned>(std::max(24.0f, 16.0f + winSize.y * 0.026f));

                auto drawLine = [&](const std::wstring& content, float alphaFactor, float yOffset) {
                    sf::Text text(hudFont, content, textSize);
                    text.setPosition(sf::Vector2f(marginX, bodyY + yOffset));
                    const float pulse = 0.94f + 0.06f * std::sin(t * 2.1f);
                    const std::uint8_t alpha = static_cast<std::uint8_t>(255.0f * clamp01(alphaFactor) * pulse);
                    text.setFillColor(sf::Color(42, 36, 30, alpha));
                    // 轻微描边感：底层浅色偏移一像素（深色字用亮边）
                    sf::Text shadow(hudFont, content, textSize);
                    shadow.setPosition(sf::Vector2f(marginX + 1.5f, bodyY + yOffset + 1.5f));
                    shadow.setFillColor(sf::Color(255, 250, 235, static_cast<std::uint8_t>(alpha * 0.22f)));
                    window.draw(shadow);
                    window.draw(text);
                };

                drawLine(lines[static_cast<size_t>(currentLine)].text, 1.0f - blendToNext, 0.0f);
                if (canBlendNext) {
                    drawLine(lines[static_cast<size_t>(currentLine + 1)].text, blendToNext, -5.0f);
                }

                sf::Text hint(hudFont, L"按 ESC 跳过", 18);
                hint.setPosition(sf::Vector2f(winSize.x - marginX - kHintFromRight, winSize.y - kHintFromBottom));
                hint.setFillColor(sf::Color(88, 82, 74, 220));
                window.draw(hint);
            }

            sf::RectangleShape fadeMask(winSize);
            if (t < kSceneInDuration) {
                const float a = 255.0f * (1.0f - clamp01(t / kSceneInDuration));
                fadeMask.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(a)));
                window.draw(fadeMask);
            } else if (t > totalDuration - kSceneOutDuration) {
                const float a = 255.0f * clamp01((t - (totalDuration - kSceneOutDuration)) / kSceneOutDuration);
                fadeMask.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(a)));
                window.draw(fadeMask);
            }

            window.display();

            if (ironclad_map_preload_host) {
                // 前若干帧每帧少解码几张，减轻卡顿；之后略加快仍控制单帧负载。
                const int budget = (ironcladPreloadStoryFrames < 12) ? 3 : 10;
                ironclad_map_preload_host->tick_ironclad_attack_anim_map_preload_frame(budget);
                ++ironcladPreloadStoryFrames;
            }
        }
    }

    } // namespace

    void GameFlowController::runBattleEntryAnimation() {
        const int mapIdx = mapConfigManager_.getCurrentIndex();
        std::vector<StorySceneLine> introText;
        if (mapIdx == 0) {
            introText = {
                { L"溯源者，你自未来而来，当知华夏文明之根，始于先秦。" },
                { L"然此世之灵，已为异气所染，山河失色，礼乐蒙尘。" },
                { L"你需以卡牌为剑，以智慧为盾，净化异灵，重铸文明之光。" },
                { L"记住，每一步选择，皆是命运之轮。" }
            };
        } else if (mapIdx == 1) {
            introText = {
                { L"溯源者，你已踏入汉唐盛世，这是中华文明最璀璨的篇章。" },
                { L"丝绸之路的驼铃，长安城的繁华，皆在你眼前展开。" },
                { L"然异灵亦潜藏于盛世之下，你需以卡牌为笔，书写净化之章。" }
            };
        } else {
            introText = {
                { L"溯源者，你已来到宋明之世，这是文化与科技并重的时代。" },
                { L"活字印刷的墨香，青花瓷的雅致，皆在你眼前展现。" },
                { L"然异灵亦侵蚀于此，你需以卡牌为舟，渡文明之河。" }
            };
        }
        runStoryDialogAnimationImpl(
            window_,
            this,
            hudFont_,
            hudFontLoaded_,
            introText,
            resolveCinematicBackgroundPath(mapIdx, "")
        );
    }

    void GameFlowController::runCinematicDialog(const std::vector<std::wstring>& lines, const std::string& backgroundPath) {
        std::vector<StorySceneLine> converted;
        converted.reserve(lines.size());
        for (const auto& line : lines) {
            converted.push_back({ line });
        }
        runStoryDialogAnimationImpl(
            window_,
            this,
            hudFont_,
            hudFontLoaded_,
            converted,
            resolveCinematicBackgroundPath(mapConfigManager_.getCurrentIndex(), backgroundPath)
        );
    }

} // namespace tce
