//include/MapEngine/MapUI.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include "MapEngine.hpp" 
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace MapEngine {

    class MapUI {
    public:
        MapUI();
        ~MapUI();

        bool initialize(sf::RenderWindow* window);
        bool loadNodeTexture(NodeType type, const std::string& filePath);
        void loadAllNodeTextures();
        bool loadLegendTexture(const std::string& filePath);
        bool loadBackgroundTexture(const std::string& filePath);
        void setMap(const MapEngine* engine);
        void draw();
        std::string handleClick(int mouseX, int mouseY);
        void setCurrentLayer(int layer);
        int getCurrentLayer() const { return m_currentLayer; }
        void setLegendPosition(float x, float y) { m_legendPosition = { x, y }; }
        void setLegendScale(float scale) { m_legendScale = scale; }
        void setNodeColors();

    private:
        void drawEdges();
        void drawNodes();
        void drawLegend();

        sf::Color getNodeColor(NodeType type, bool isVisited, bool isCurrent);
        bool loadFonts();

    private:
        sf::RenderWindow* m_window;
        const MapEngine* m_mapEngine;

        // 节点图片
        std::unordered_map<NodeType, sf::Texture> m_nodeTextures;
        std::unordered_map<NodeType, bool> m_textureLoaded;

        // 图例图片
        sf::Texture m_legendTexture;
        sf::Sprite m_legendSprite;
        bool m_legendLoaded;

        // 背景图片 - 使用指针延迟构造
        sf::Texture m_backgroundTexture;
        sf::Sprite* m_backgroundSprite;  // 改为指针
        bool m_backgroundLoaded;

        // 节点半径
        const float NODE_RADIUS = 45.0f;
        const float SELECTED_RADIUS = 54.0f;

        // 颜色定义
        sf::Color colorEnemy;
        sf::Color colorElite;
        sf::Color colorEvent;
        sf::Color colorRest;
        sf::Color colorMerchant;
        sf::Color colorTreasure;
        sf::Color colorBoss;
        sf::Color colorVisited;
        sf::Color colorCurrent;
        sf::Color colorUnreachable;
        sf::Color colorEdge;

        sf::Font m_font;
        int m_currentLayer;
        bool m_showNodeIds;
        sf::Vector2f m_legendPosition;
        float m_legendScale;
    };

}
