// src/MapEngine/MapUI.cpp
#include "../../include/MapEngine/MapUI.hpp"
#include "../Common/NodeTypes.hpp"
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

        if (!loadFonts()) {
            std::cerr << "警告：无法加载字体，将不显示文字" << std::endl;
            m_showNodeIds = false;
        }

        loadAllNodeTextures();
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

            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(fromNode.position.x, fromNode.position.y), colorEdge),
                sf::Vertex(sf::Vector2f(toNode.position.x, toNode.position.y), colorEdge)
            };

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

        // 先绘制背景（如果存在）
        if (m_backgroundLoaded && m_backgroundSprite) {
            m_window->draw(*m_backgroundSprite);
        }
     
        // 再绘制地图内容
        if (m_mapEngine) {
            drawEdges();
            drawNodes();
        }

        // 最后绘制图例
        drawLegend();

    }

    std::string MapUI::handleClick(int mouseX, int mouseY) {
        if (!m_mapEngine) return "";

        auto snapshot = m_mapEngine->get_map_snapshot();

        for (const auto& node : snapshot.all_nodes) {
            float dx = mouseX - node.position.x;
            float dy = mouseY - node.position.y;
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance <= NODE_RADIUS + 10) {
                std::cout << "点击到节点: " << node.id << std::endl;
                return node.id;
            }
        }
        return "";
    }

}