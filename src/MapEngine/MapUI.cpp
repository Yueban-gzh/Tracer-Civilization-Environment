// src/MapEngine/MapUI.cpp
#include "../../include/MapEngine/MapUI.hpp"
#include "../../include/Common/NodeTypes.hpp"
#include <iostream>
#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <filesystem>

namespace MapEngine {

    static std::string resolve_asset_path_fallback(const std::string& rel) {
        namespace fs = std::filesystem;
        fs::path p = fs::u8path(rel);
        if (fs::exists(p)) return p.u8string();

        // 常见：从 x64/Debug 启动，工作目录不在项目根
        fs::path cur = fs::current_path();
        for (int i = 0; i < 6; ++i) {
            fs::path cand = cur / p;
            if (fs::exists(cand)) return cand.u8string();
            if (!cur.has_parent_path()) break;
            cur = cur.parent_path();
        }
        return rel; // 让调用方走原始路径并输出警告
    }

    MapUI::MapUI()
        : m_window(nullptr)
        , m_mapEngine(nullptr)
        , m_currentLayer(0)
        , m_showNodeIds(true)
        , m_legendLoaded(false)
        , m_legendPosition(1600.0f, 100.0f)
        , m_legendScale(1.0f)
        , m_legendTexture()
        , m_legendSprite(m_legendTexture)
        , m_backgroundLoaded(false)
        , m_backgroundTexture()
        , m_backgroundSprite(nullptr)  // 初始化为空指针
    {
        setNodeColors();
        for (int i = 0; i <= static_cast<int>(NodeType::Boss); ++i) {
            m_textureLoaded[static_cast<NodeType>(i)] = false;
        }
    }

    MapUI::~MapUI() {
        if (m_backgroundSprite) {
            delete m_backgroundSprite;  // 释放内存
        }
    }



    void MapUI::setNodeColors() {
        colorCurrent = sf::Color(0, 0, 0);           // 黑色 - 当前节点
        colorEdge = sf::Color(0, 0, 0, 255);          // 黑色，完全不透明
    }

    // 加载图例图片
    bool MapUI::loadLegendTexture(const std::string& filePath) {
        std::cout << "===== 加载图例 =====" << std::endl;
        std::cout << "文件路径: " << filePath << std::endl;

        if (!m_legendTexture.loadFromFile(filePath)) {
            std::cerr << "图例加载失败！" << std::endl;
            m_legendLoaded = false;
            return false;
        }

        sf::Vector2u texSize = m_legendTexture.getSize();
        std::cout << "纹理加载成功，大小: " << texSize.x << "x" << texSize.y << std::endl;

        // 关键：重新创建 Sprite 并设置纹理
        // 注意：不能直接赋值，需要重新构造
        m_legendSprite = sf::Sprite(m_legendTexture);

        m_legendLoaded = true;
        std::cout << "图例加载完成" << std::endl;
        std::cout << "===== 加载结束 =====\n" << std::endl;

        return true;
    }

    bool MapUI::loadBackgroundTexture(const std::string& filePath) {
        std::cout << "加载背景: " << filePath << std::endl;

        if (!m_backgroundTexture.loadFromFile(filePath)) {
            std::cerr << "背景加载失败！" << std::endl;
            m_backgroundLoaded = false;
            return false;
        }

        // 如果之前有 Sprite，先删除
        if (m_backgroundSprite) {
            delete m_backgroundSprite;
        }

        // 用加载好的纹理创建新的 Sprite
        m_backgroundSprite = new sf::Sprite(m_backgroundTexture);

        // 缩放背景以适应窗口
        if (m_window && m_backgroundSprite) {
            sf::Vector2u windowSize = m_window->getSize();
            sf::Vector2u textureSize = m_backgroundTexture.getSize();

            float scaleX = static_cast<float>(windowSize.x) / textureSize.x;
            float scaleY = static_cast<float>(windowSize.y) / textureSize.y;
            float scale = std::max(scaleX, scaleY);

            m_backgroundSprite->setScale(sf::Vector2f(scale, scale));
        }

        m_backgroundLoaded = true;
        std::cout << "背景加载成功，大小: "
            << m_backgroundTexture.getSize().x << "x"
            << m_backgroundTexture.getSize().y << std::endl;
        return true;
    }

    // 绘制图例
    void MapUI::drawLegend() {
        if (!m_window) {
            std::cout << "drawLegend: m_window为空" << std::endl;
            return;
        }

        if (!m_legendLoaded) {
            std::cout << "drawLegend: m_legendLoaded = false" << std::endl;
            return;
        }

        sf::Vector2u texSize = m_legendTexture.getSize();
        if (texSize.x == 0 || texSize.y == 0) {
            std::cout << "drawLegend: 纹理无效，大小: " << texSize.x << "x" << texSize.y << std::endl;
            return;
        }

        std::cout << "drawLegend: 开始绘制" << std::endl;
        std::cout << "  纹理大小: " << texSize.x << "x" << texSize.y << std::endl;
        std::cout << "  位置: (" << m_legendPosition.x << ", " << m_legendPosition.y << ")" << std::endl;
        std::cout << "  缩放: " << m_legendScale << std::endl;

        // 确保 Sprite 使用的是最新的纹理
        m_legendSprite.setPosition(sf::Vector2f(m_legendPosition.x, m_legendPosition.y));
        m_legendSprite.setScale(sf::Vector2f(m_legendScale, m_legendScale));

        m_window->draw(m_legendSprite);
        std::cout << "drawLegend: 绘制完成" << std::endl;
    }

    bool MapUI::loadNodeTexture(NodeType type, const std::string& filePath) {
        sf::Texture texture;
        const std::string resolved = resolve_asset_path_fallback(filePath);
        if (!texture.loadFromFile(resolved)) {
            std::cerr << "警告：无法加载图片 " << filePath << std::endl;
            m_textureLoaded[type] = false;
            return false;
        }

        m_nodeTextures[type] = texture;
        m_textureLoaded[type] = true;
        std::cout << "成功加载图片: " << resolved << std::endl;
        return true;
    }

    void MapUI::loadAllNodeTextures() {
        loadNodeTexture(NodeType::Enemy, "assets/images/enemy.png");
        loadNodeTexture(NodeType::Elite, "assets/images/elite.png");
        loadNodeTexture(NodeType::Event, "assets/images/event.png");
        loadNodeTexture(NodeType::Rest, "assets/images/rest.png");
        loadNodeTexture(NodeType::Merchant, "assets/images/merchant.png");
        loadNodeTexture(NodeType::Treasure, "assets/images/treasure.png");
        loadNodeTexture(NodeType::Boss, "assets/images/boss.png");
    }

    bool MapUI::initialize(sf::RenderWindow* window) {
        if (!window) {
            std::cerr << "错误：窗口指针为空" << std::endl;
            return false;
        }

        m_window = window;

        // 初始化滚动变量
        m_viewOffset = 0.0f;
        m_minOffset = 0.0f;
        m_maxOffset = 0.0f;

        if (!loadFonts()) {
            std::cerr << "警告：无法加载字体，将不显示文字" << std::endl;
            m_showNodeIds = false;
        }

        loadAllNodeTextures();

        // 【新增】加载已访问节点覆盖层图片
        {
            const std::string path = resolve_asset_path_fallback("assets/images/visited_overlay.png");
            if (!m_visitedOverlayTexture.loadFromFile(path)) {
                std::cerr << "警告：无法加载已访问节点覆盖层图片 assets/images/visited_overlay.png" << std::endl;
            m_visitedOverlayLoaded = false;
            }
            else {
                m_visitedOverlayLoaded = true;
                std::cout << "成功加载已访问节点覆盖层图片: " << path << std::endl;
            }
        }

        return true;
    }

    bool MapUI::loadFonts() {
        std::vector<std::string> fontPaths = {
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/simhei.ttf",
            "C:/Windows/Fonts/msyh.ttc",
        };

        for (const auto& path : fontPaths) {
            if (m_font.openFromFile(path)) {
                return true;
            }
        }
        return false;
    }

    // MapUI.cpp - setMap函数
    void MapUI::setMap(const MapEngine* engine) {
        m_mapEngine = engine;

        if (engine) {
            auto snapshot = engine->get_map_snapshot();
            float minY = 10000.0f, maxY = -10000.0f;
            for (const auto& node : snapshot.all_nodes) {
                minY = std::min(minY, node.position.y);
                maxY = std::max(maxY, node.position.y);
            }

            const float mapHeight = maxY - minY;
            const float windowHeight =
                m_window ? static_cast<float>(m_window->getSize().y) : 1080.0f;
            // 顶部状态栏会遮挡地图可见区域，需要预留滚动余量。
            constexpr float kTopHudReserve = 110.0f;
            constexpr float kBottomReserve = 24.0f;
            const float visibleHeight = std::max(200.0f, windowHeight - kTopHudReserve - kBottomReserve);

            // 由于层间距增大到 120，12层总高度 = 11 * 120 = 1320 像素
            // 加上起始偏移，总高度约 1400 像素
            if (mapHeight > visibleHeight) {
                // 允许向上滚动看到 Boss 层
                m_minOffset = -100.0f;
                // 允许向下滚动看到底部
                m_maxOffset = mapHeight - visibleHeight + 150.0f;
            }
            else {
                m_minOffset = -50.0f;
                m_maxOffset = visibleHeight - mapHeight + 50.0f;
            }

            // 初始偏移为0，显示底部（第0层）
            m_viewOffset = -300.0f;

            std::cout << "=== 地图滚动范围 ===" << std::endl;
            std::cout << "地图Y范围: [" << minY << ", " << maxY << "]" << std::endl;
            std::cout << "地图高度: " << mapHeight << std::endl;
            std::cout << "窗口高度: " << windowHeight << " 可见高度: " << visibleHeight << std::endl;
            std::cout << "滚动范围: [" << m_minOffset << ", " << m_maxOffset << "]" << std::endl;
            std::cout << "===================" << std::endl;
        }
    }

    void MapUI::setCurrentLayer(int layer) {
        m_currentLayer = layer;
    }

    sf::Color MapUI::getNodeColor(NodeType type, bool isVisited, bool isCurrent) {
        if (isCurrent) return colorCurrent;
        if (isVisited) return colorVisited;

        switch (type) {
        case NodeType::Enemy:     return colorEnemy;
        case NodeType::Elite:     return colorElite;
        case NodeType::Event:     return colorEvent;
        case NodeType::Rest:      return colorRest;
        case NodeType::Merchant:  return colorMerchant;
        case NodeType::Treasure:  return colorTreasure;
        case NodeType::Boss:      return colorBoss;
        default: return sf::Color::White;
        }
    }

    void MapUI::drawEdges() {
        if (!m_mapEngine || !m_window) return;

        auto snapshot = m_mapEngine->get_map_snapshot();

        const float thickness = 4.0f;
        const float dashLen = 18.0f;
        const float gapLen = 12.0f;

        for (const auto& edge : snapshot.all_edges) {
            auto fromNode = m_mapEngine->get_node_by_id(edge.from);
            auto toNode = m_mapEngine->get_node_by_id(edge.to);

            const sf::Vector2f a(fromNode.position.x, fromNode.position.y);
            const sf::Vector2f b(toNode.position.x, toNode.position.y);

            // 轻微弧度：二次贝塞尔曲线 a -> c -> b
            const sf::Vector2f d = b - a;
            const float chord = std::sqrt(d.x * d.x + d.y * d.y);
            if (chord <= 1.0f) continue;
            const sf::Vector2f dir(d.x / chord, d.y / chord);
            const sf::Vector2f perp(-dir.y, dir.x);

            uint32_t h = 2166136261u;
            for (char cch : edge.from) h = (h ^ (uint8_t)cch) * 16777619u;
            for (char cch : edge.to) h = (h ^ (uint8_t)cch) * 16777619u;
            const float sign = (h & 1u) ? 1.0f : -1.0f;
            const float curve = sign * std::min(44.0f, chord * 0.12f);
            const sf::Vector2f c = (a + b) * 0.5f + perp * curve;

            auto bez = [&](float t) {
                const float u = 1.0f - t;
                return a * (u * u) + c * (2.0f * u * t) + b * (t * t);
            };

            // 采样成折线，再在折线上画粗虚线（避免棱角分明）
            constexpr int N = 26;
            std::vector<sf::Vector2f> pts;
            pts.reserve(N + 1);
            for (int i = 0; i <= N; ++i) {
                float t = (float)i / (float)N;
                pts.push_back(bez(t));
            }

            bool drawing = true;
            float remain = dashLen;
            for (int i = 0; i < (int)pts.size() - 1; ++i) {
                sf::Vector2f p0 = pts[i];
                sf::Vector2f p1 = pts[i + 1];
                sf::Vector2f sd = p1 - p0;
                float segLen = std::sqrt(sd.x * sd.x + sd.y * sd.y);
                if (segLen <= 0.001f) continue;
                sf::Vector2f sdir(sd.x / segLen, sd.y / segLen);
                float angleDeg = std::atan2(sd.y, sd.x) * 180.0f / 3.14159265f;

                float pos = 0.0f;
                while (pos < segLen) {
                    float step = std::min(remain, segLen - pos);
                    if (step <= 0.001f) break;

                    if (drawing) {
                        sf::RectangleShape dash(sf::Vector2f(step, thickness));
                        dash.setFillColor(colorEdge);
                        dash.setOrigin(sf::Vector2f(0.0f, thickness * 0.5f));
                        dash.setPosition(p0 + sdir * pos);
                        dash.setRotation(sf::degrees(angleDeg));
                        m_window->draw(dash);
                    }

                    pos += step;
                    remain -= step;
                    if (remain <= 0.001f) {
                        drawing = !drawing;
                        remain = drawing ? dashLen : gapLen;
                    }
                }
            }
        }
    }

    void MapUI::drawNodes() {
        if (!m_mapEngine || !m_window) return;

        auto snapshot = m_mapEngine->get_map_snapshot();

        for (const auto& node : snapshot.all_nodes) {
            float radius = (node.layer == m_currentLayer) ? SELECTED_RADIUS : NODE_RADIUS;

            if (m_textureLoaded[node.type]) {
                sf::Sprite sprite(m_nodeTextures[node.type]);
                sf::FloatRect bounds = sprite.getLocalBounds();

                float scale;
                float displayRadius = radius;
                float extraScale = 1.0f;

                // 可达节点：呼吸闪烁提示（只对“下一步可进入”的节点）
                if (node.is_reachable && !node.is_current && !node.is_completed) {
                    // 给每个节点一点点相位差，避免全部同步
                    uint32_t h = 2166136261u;
                    for (char c : node.id) h = (h ^ (uint8_t)c) * 16777619u;
                    const float phase = (h % 1000) * 0.001f * 6.2831853f;
                    const float pulse = 0.5f + 0.5f * std::sin(m_timeSec * 4.2f + phase);
                    extraScale *= (1.0f + 0.26f * pulse);
                }

                // 悬停：插值放大（hoverBlend 0..1）
                if (!m_hoveredNodeId.empty() && node.id == m_hoveredNodeId) {
                    // smoothstep
                    const float t = m_hoverBlend;
                    const float s = t * t * (3.0f - 2.0f * t);
                    extraScale *= (1.0f + 0.28f * s);
                }

                if (node.type == NodeType::Boss) {
                    scale = (radius * 2 * 7.0f) / bounds.size.x;
                    displayRadius = radius * 7.0f;
                }
                else if (node.type == NodeType::Elite) {
                    scale = (radius * 2 * 1.8f) / bounds.size.x;
                    displayRadius = radius * 1.8f;
                }
                else if (node.type == NodeType::Enemy) {
                    scale = (radius * 2 * 0.8f) / bounds.size.x;
                    displayRadius = radius * 0.8f;
                }
                else if (node.type == NodeType::Event) {
                    scale = (radius * 2 * 0.6f) / bounds.size.x;
                    displayRadius = radius * 0.6f;
                }
                else {
                    scale = (radius * 2) / bounds.size.x;
                }

                sprite.setScale(sf::Vector2f(scale * extraScale, scale * extraScale));
                sf::FloatRect scaledBounds = sprite.getLocalBounds();
                sprite.setOrigin(sf::Vector2f(scaledBounds.size.x / 2.0f, scaledBounds.size.y / 2.0f));
                sprite.setPosition(sf::Vector2f(node.position.x, node.position.y));

                m_window->draw(sprite);

                // 【新增】已访问节点覆盖图片
                if (m_visitedOverlayLoaded && node.is_visited && !node.is_current) {
                    sf::Sprite overlaySprite(m_visitedOverlayTexture);
                    sf::FloatRect bounds = overlaySprite.getLocalBounds();

                    // 缩放覆盖层匹配节点当前视觉大小（包含 extraScale）
                    // 访问覆盖层做得更“夸张”一点：明显大于节点本体
                    const float targetW = (displayRadius * extraScale) * 5.25f;
                    float scale = targetW / std::max(1.0f, bounds.size.x);
                    overlaySprite.setScale(sf::Vector2f(scale, scale));

                    // 居中定位
                    sf::FloatRect scaledBounds = overlaySprite.getLocalBounds();
                    overlaySprite.setOrigin(sf::Vector2f(scaledBounds.size.x / 2.0f,
                        scaledBounds.size.y / 2.0f));
                    overlaySprite.setPosition(sf::Vector2f(node.position.x, node.position.y));

                    // 可选：半透明效果（0-255，255为不透明）
                    // 叠加层本身是“笔刷圆环”，为避免看起来像普通圆形描边，这里轻微提亮并加透明度
                    overlaySprite.setColor(sf::Color(255, 255, 255, 235));

                    m_window->draw(overlaySprite);
                }

                if (node.is_current) {
                    const float r = displayRadius * extraScale;
                    const float arrowW = std::max(18.0f, r * 0.75f);
                    const float arrowH = std::max(16.0f, r * 0.65f);
                    const sf::Vector2f base(node.position.x, node.position.y - r - 18.0f);

                    sf::ConvexShape arrow(3);
                    arrow.setPoint(0, sf::Vector2f(0.0f, 0.0f));
                    arrow.setPoint(1, sf::Vector2f(arrowW, 0.0f));
                    arrow.setPoint(2, sf::Vector2f(arrowW * 0.5f, arrowH));
                    arrow.setOrigin(sf::Vector2f(arrowW * 0.5f, arrowH * 0.15f));
                    arrow.setPosition(base);
                    arrow.setFillColor(sf::Color(255, 220, 120, 240));
                    arrow.setOutlineColor(sf::Color(30, 18, 12, 220));
                    arrow.setOutlineThickness(2.0f);

                    sf::ConvexShape shadow = arrow;
                    shadow.setPosition(sf::Vector2f(base.x + 2.0f, base.y + 2.0f));
                    shadow.setFillColor(sf::Color(0, 0, 0, 90));
                    shadow.setOutlineThickness(0.0f);
                    m_window->draw(shadow);
                    m_window->draw(arrow);
                }

                // 可达节点额外提示圈（叠在最上层，颜色轻一点）
                if (node.is_reachable && !node.is_current && !node.is_completed) {
                    uint32_t h = 2166136261u;
                    for (char c : node.id) h = (h ^ (uint8_t)c) * 16777619u;
                    const float phase = (h % 1000) * 0.001f * 6.2831853f;
                    const float pulse = 0.5f + 0.5f * std::sin(m_timeSec * 4.2f + phase);
                    const int a = 90 + (int)std::lround(120.0f * pulse);

                    const float ringR = (displayRadius * extraScale) + 26.0f;
                    sf::CircleShape ring(ringR);
                    ring.setFillColor(sf::Color::Transparent);
                    ring.setOutlineThickness(5.0f);
                    const int aClamped = std::max(0, std::min(255, a));
                    ring.setOutlineColor(sf::Color(255, 235, 170, static_cast<std::uint8_t>(aClamped)));
                    ring.setOrigin(sf::Vector2f(ringR, ringR));
                    ring.setPosition(sf::Vector2f(node.position.x, node.position.y));
                    m_window->draw(ring);
                }
            }
            else {
                sf::CircleShape circle(radius);
                circle.setOrigin(sf::Vector2f(radius, radius));
                circle.setPosition(sf::Vector2f(node.position.x, node.position.y));

                sf::Color nodeColor = getNodeColor(node.type, node.is_visited, node.is_current);

                if (!node.is_reachable && !node.is_current) {
                    nodeColor = sf::Color(
                        nodeColor.r / 2 + 128,
                        nodeColor.g / 2 + 128,
                        nodeColor.b / 2 + 128
                    );
                }

                circle.setFillColor(nodeColor);
                circle.setOutlineThickness(2.0f);

                if (!node.is_current) {
                    circle.setOutlineColor(sf::Color(100, 100, 100));
                }

                m_window->draw(circle);

                // 已访问节点覆盖贴图：即使没有加载节点图标，也叠一层 visited_overlay.png
                if (m_visitedOverlayLoaded && node.is_visited && !node.is_current) {
                    sf::Sprite overlaySprite(m_visitedOverlayTexture);
                    sf::FloatRect bounds = overlaySprite.getLocalBounds();
                    const float scale = (radius * 2.0f * 3.10f) / std::max(1.0f, bounds.size.x);
                    overlaySprite.setScale(sf::Vector2f(scale, scale));
                    sf::FloatRect sb = overlaySprite.getLocalBounds();
                    overlaySprite.setOrigin(sf::Vector2f(sb.size.x / 2.0f, sb.size.y / 2.0f));
                    overlaySprite.setPosition(sf::Vector2f(node.position.x, node.position.y));
                    overlaySprite.setColor(sf::Color(255, 255, 255, 235));
                    m_window->draw(overlaySprite);
                }

                if (node.is_current) {
                    const float r = radius;
                    const float arrowW = std::max(18.0f, r * 0.75f);
                    const float arrowH = std::max(16.0f, r * 0.65f);
                    const sf::Vector2f base(node.position.x, node.position.y - r - 18.0f);

                    sf::ConvexShape arrow(3);
                    arrow.setPoint(0, sf::Vector2f(0.0f, 0.0f));
                    arrow.setPoint(1, sf::Vector2f(arrowW, 0.0f));
                    arrow.setPoint(2, sf::Vector2f(arrowW * 0.5f, arrowH));
                    arrow.setOrigin(sf::Vector2f(arrowW * 0.5f, arrowH * 0.15f));
                    arrow.setPosition(base);
                    arrow.setFillColor(sf::Color(255, 220, 120, 240));
                    arrow.setOutlineColor(sf::Color(30, 18, 12, 220));
                    arrow.setOutlineThickness(2.0f);

                    sf::ConvexShape shadow = arrow;
                    shadow.setPosition(sf::Vector2f(base.x + 2.0f, base.y + 2.0f));
                    shadow.setFillColor(sf::Color(0, 0, 0, 90));
                    shadow.setOutlineThickness(0.0f);
                    m_window->draw(shadow);
                    m_window->draw(arrow);
                }
            }

            if (m_showNodeIds && m_font.getInfo().family != "" && !m_textureLoaded[node.type]) {
                sf::Text text(m_font);

                std::string symbol;
                switch (node.type) {
                case NodeType::Enemy:     symbol = "E"; break;
                case NodeType::Elite:     symbol = "精英"; break;
                case NodeType::Event:     symbol = "?"; break;
                case NodeType::Rest:      symbol = "休"; break;
                case NodeType::Merchant:  symbol = "商"; break;
                case NodeType::Treasure:  symbol = "宝"; break;
                case NodeType::Boss:      symbol = "首"; break;
                }

                text.setString(sf::String::fromUtf8(symbol.begin(), symbol.end()));
                text.setCharacterSize(20);

                if (node.is_current) {
                    text.setFillColor(sf::Color::White);
                    text.setOutlineColor(sf::Color::Black);
                }
                else {
                    text.setFillColor(sf::Color::Black);
                    text.setOutlineColor(sf::Color::White);
                }
                text.setOutlineThickness(1.0f);

                sf::FloatRect textRect = text.getLocalBounds();
                text.setOrigin(sf::Vector2f(textRect.position.x + textRect.size.x / 2.0f,
                    textRect.position.y + textRect.size.y / 2.0f));
                text.setPosition(sf::Vector2f(node.position.x, node.position.y));

                m_window->draw(text);
            }
        }
    }

    void MapUI::draw() {
        if (!m_window) return;

        // 先绘制背景（在原视图，不滚动）
        sf::View originalView = m_window->getView();

        if (m_backgroundLoaded && m_backgroundSprite) {
            m_window->draw(*m_backgroundSprite);
        }

        // ====== 动画时钟（用于悬停插值、可达闪烁等）======
        const float nowSec = m_animClock.getElapsedTime().asSeconds();
        float dt = nowSec - m_lastAnimSec;
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.05f) dt = 0.05f; // 防止卡顿导致插值跳变
        m_lastAnimSec = nowSec;
        m_timeSec = nowSec;

        // 创建滚动视图
        sf::View scrollView = originalView;
        scrollView.move(sf::Vector2f(0.0f, -m_viewOffset));
        m_window->setView(scrollView);

        // ====== 悬停检测：在滚动视图下把鼠标转世界坐标 ======
        if (m_mapEngine) {
            const sf::Vector2i mp = sf::Mouse::getPosition(*m_window);
            const sf::Vector2f worldPos = m_window->mapPixelToCoords(mp);

            std::string bestId;
            float bestDist = 1e9f;

            const auto snapshot = m_mapEngine->get_map_snapshot();
            for (const auto& node : snapshot.all_nodes) {
                float visualRadius = NODE_RADIUS;
                if (node.type == NodeType::Boss) visualRadius = NODE_RADIUS * 7.0f;
                else if (node.type == NodeType::Elite) visualRadius = NODE_RADIUS * 1.8f;
                else if (node.type == NodeType::Enemy) visualRadius = NODE_RADIUS * 0.8f;
                else if (node.type == NodeType::Event) visualRadius = NODE_RADIUS * 0.6f;

                const float hoverRadius = visualRadius + 24.0f;
                const float dx = worldPos.x - node.position.x;
                const float dy = worldPos.y - node.position.y;
                const float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > hoverRadius) continue;

                // 悬停动画：不可达节点也允许悬停（仅用于视觉反馈，不改变点击规则）
                if (!m_allowAnyNodeClick) {
                    if (node.is_completed) continue;
                    if (node.is_current) continue;
                }

                if (dist < bestDist) {
                    bestDist = dist;
                    bestId = node.id;
                }
            }

            // 根据 hover 结果更新插值
            const float k = 12.0f;
            const float step = 1.0f - std::exp(-k * dt);

            const bool hovering = !bestId.empty();
            if (hovering) {
                if (m_hoveredNodeId != bestId) {
                    // 换目标时先从较低值开始，避免突兀
                    m_hoveredNodeId = bestId;
                    m_hoverBlend = std::min(m_hoverBlend, 0.35f);
                }
                m_hoverBlend = m_hoverBlend + (1.0f - m_hoverBlend) * step;
            }
            else {
                m_hoverBlend = m_hoverBlend + (0.0f - m_hoverBlend) * step;
                if (m_hoverBlend < 0.02f) {
                    m_hoverBlend = 0.0f;
                    m_hoveredNodeId.clear();
                }
            }
        }

        // 绘制地图内容
        if (m_mapEngine) {
            drawEdges();
            drawNodes();
        }

        // 恢复原视图绘制图例
        m_window->setView(originalView);
        drawLegend();
    }

    // MapUI.cpp - handleClick()

    std::string MapUI::handleClick(int mouseX, int mouseY) {
        if (!m_nodesClickable) return "";
        if (!m_mapEngine || !m_window) return "";

        // 【关键】保存当前视图，设置滚动视图，转换坐标，然后恢复
        sf::View originalView = m_window->getView();

        // 创建与 draw() 中相同的滚动视图
        sf::View scrollView = originalView;
        scrollView.move(sf::Vector2f(0.0f, -m_viewOffset));

        // 使用滚动视图转换鼠标坐标
        m_window->setView(scrollView);
        sf::Vector2f worldPos = m_window->mapPixelToCoords(sf::Vector2i(mouseX, mouseY));

        // 立即恢复原始视图（不影响后续绘制）
        m_window->setView(originalView);

        // 获取当前是否有已选节点
        bool hasCurrent = m_mapEngine->hasCurrentNode();

        auto snapshot = m_mapEngine->get_map_snapshot();

        for (const auto& node : snapshot.all_nodes) {
            // 计算视觉半径
            float visualRadius = NODE_RADIUS;
            if (node.type == NodeType::Boss) visualRadius = NODE_RADIUS * 7.0f;
            else if (node.type == NodeType::Elite) visualRadius = NODE_RADIUS * 1.8f;
            else if (node.type == NodeType::Enemy) visualRadius = NODE_RADIUS * 0.8f;
            else if (node.type == NodeType::Event) visualRadius = NODE_RADIUS * 0.6f;

            float clickRadius = visualRadius + 30.0f;  // 增大点击范围

            float dx = worldPos.x - node.position.x;
            float dy = worldPos.y - node.position.y;
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance <= clickRadius) {
                // 状态检查
                if (node.is_completed && !m_allowAnyNodeClick) {
                    std::cout << "节点 " << node.id << " 已完成" << std::endl;
                    return "";
                }
                if (node.is_current) {
                    std::cout << "已经在节点 " << node.id << std::endl;
                    return "";
                }
                if (!m_allowAnyNodeClick && !node.is_reachable && !(node.layer == 0 && !hasCurrent)) {
                    std::cout << "节点 " << node.id << " 不可达" << std::endl;
                    return "";
                }

                std::cout << "点击到节点: " << node.id << " 类型:" << static_cast<int>(node.type) << std::endl;
                return node.id;
            }
        }
        return "";
    }

    void MapUI::scroll(float delta) {
        m_viewOffset = std::max(m_minOffset, std::min(m_maxOffset, m_viewOffset + delta));
    }

}
