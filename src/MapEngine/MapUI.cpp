// src/MapEngine/MapUI.cpp
#include "../../include/MapEngine/MapUI.hpp"
#include "../../include/Common/NodeTypes.hpp"
#include <iostream>
#include <cmath>
#include <vector>

namespace MapEngine {

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
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "警告：无法加载图片 " << filePath << std::endl;
            m_textureLoaded[type] = false;
            return false;
        }

        m_nodeTextures[type] = texture;
        m_textureLoaded[type] = true;
        std::cout << "成功加载图片: " << filePath << std::endl;
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
        if (!m_visitedOverlayTexture.loadFromFile("assets/images/visited_overlay.png")) {
            std::cerr << "警告：无法加载已访问节点覆盖层图片 assets/images/visited_overlay.png" << std::endl;
            m_visitedOverlayLoaded = false;
        }
        else {
            m_visitedOverlayLoaded = true;
            std::cout << "成功加载已访问节点覆盖层图片" << std::endl;
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
            m_viewOffset = 0.0f;

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

        for (const auto& edge : snapshot.all_edges) {
            auto fromNode = m_mapEngine->get_node_by_id(edge.from);
            auto toNode = m_mapEngine->get_node_by_id(edge.to);

            sf::Vertex line[2];
            // 第一个顶点：位置 + 颜色
            line[0].position = sf::Vector2f(fromNode.position.x, fromNode.position.y);
            line[0].color = colorEdge;
            // 第二个顶点：位置 + 颜色
            line[1].position = sf::Vector2f(toNode.position.x, toNode.position.y);
            line[1].color = colorEdge;

            m_window->draw(line, 2, sf::PrimitiveType::Lines);
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

                if (node.type == NodeType::Boss) {
                    scale = (radius * 2 * 3.5f) / bounds.size.x;
                    displayRadius = radius * 3.5f;
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

                sprite.setScale(sf::Vector2f(scale, scale));
                sf::FloatRect scaledBounds = sprite.getLocalBounds();
                sprite.setOrigin(sf::Vector2f(scaledBounds.size.x / 2.0f, scaledBounds.size.y / 2.0f));
                sprite.setPosition(sf::Vector2f(node.position.x, node.position.y));

                m_window->draw(sprite);

                // 【新增】已访问节点覆盖图片
                if (m_visitedOverlayLoaded && node.is_visited && !node.is_current) {
                    sf::Sprite overlaySprite(m_visitedOverlayTexture);
                    sf::FloatRect bounds = overlaySprite.getLocalBounds();

                    // 缩放覆盖层匹配节点视觉大小（可调整系数）
                    float scale = (displayRadius * 3.5f) / bounds.size.x;
                    overlaySprite.setScale(sf::Vector2f(scale, scale));

                    // 居中定位
                    sf::FloatRect scaledBounds = overlaySprite.getLocalBounds();
                    overlaySprite.setOrigin(sf::Vector2f(scaledBounds.size.x / 2.0f,
                        scaledBounds.size.y / 2.0f));
                    overlaySprite.setPosition(sf::Vector2f(node.position.x, node.position.y));

                    // 可选：半透明效果（0-255，255为不透明）
                    overlaySprite.setColor(sf::Color(255, 255, 255, 220));

                    m_window->draw(overlaySprite);
                }

                if (node.is_current) {
                    sf::CircleShape blackOutline(displayRadius + 3);
                    blackOutline.setFillColor(sf::Color::Transparent);
                    blackOutline.setOutlineThickness(3.0f);
                    blackOutline.setOutlineColor(sf::Color::Black);
                    blackOutline.setOrigin(sf::Vector2f(displayRadius + 3, displayRadius + 3));
                    blackOutline.setPosition(sf::Vector2f(node.position.x, node.position.y));
                    m_window->draw(blackOutline);
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

                if (node.is_current) {
                    circle.setOutlineColor(sf::Color::Black);
                }
                else {
                    circle.setOutlineColor(sf::Color(100, 100, 100));
                }

                m_window->draw(circle);

                if (node.is_current) {
                    sf::CircleShape blackOutline(radius + 3);
                    blackOutline.setFillColor(sf::Color::Transparent);
                    blackOutline.setOutlineThickness(3.0f);
                    blackOutline.setOutlineColor(sf::Color::Black);
                    blackOutline.setOrigin(sf::Vector2f(radius + 3, radius + 3));
                    blackOutline.setPosition(sf::Vector2f(node.position.x, node.position.y));
                    m_window->draw(blackOutline);
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

        // 创建滚动视图
        sf::View scrollView = originalView;
        scrollView.move(sf::Vector2f(0.0f, -m_viewOffset));
        m_window->setView(scrollView);

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
            if (node.type == NodeType::Boss) visualRadius = NODE_RADIUS * 3.5f;
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
