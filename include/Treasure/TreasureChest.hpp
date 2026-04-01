#ifndef TREASURE_CHEST_HPP
#define TREASURE_CHEST_HPP

#include <string>
#include <vector>
#include <unordered_map>  // 【修复1】必须包含这个头文件
#include <random>

namespace tce {

// 遗物结构体
struct Relic {
    std::string id;
    std::string name;
    std::string description;
    int value;
};

// 战利品结构体
struct Loot {
    Relic relic;
    bool hasGold;
    int gold;
};

// 宝箱类型枚举
enum class ChestType {
    Small,
    Medium,
    Large
};

// 宝箱状态
enum class ChestState {
    Closed,
    Open
};

// 宝箱类型配置
struct ChestTypeConfig {
    ChestType type;
    float probability; // 出现概率
    float commonRelicWeight; // 普通遗物权重
    float uncommonRelicWeight; // 罕见遗物权重
    float rareRelicWeight; // 稀有遗物权重
    int minGold; // 最小金币
    int maxGold; // 最大金币
};

// 战利品工厂类
class LootFactory {
private:
    std::vector<Relic> commonRelics_;
    std::vector<Relic> uncommonRelics_;
    std::vector<Relic> rareRelics_;
    std::vector<ChestTypeConfig> chestConfigs_;
    std::mt19937 rng_;

public:
    LootFactory();
    
    // 初始化遗物数据库
    void initializeRelicDatabase();
    
    // 创建随机宝箱类型
    ChestType createRandomChestType();
    
    // 生成战利品
    Loot generateLoot(ChestType type);
    
    // 获取宝箱类型配置
    const ChestTypeConfig& getChestConfig(ChestType type) const;
    
    // 添加自定义遗物
    void addRelic(const Relic& relic, bool isCommon, bool isUncommon, bool isRare);
};

// 宝箱类
class TreasureChest {
private:
    ChestType type_;
    ChestState state_;
    Loot loot_;
    bool isOpened_;

public:
    TreasureChest(ChestType type, LootFactory& factory);
    
    // 打开宝箱
    Loot open(LootFactory& factory);
    
    // 获取宝箱类型
    ChestType getChestType() const;
    
    // 获取宝箱状态
    ChestState getState() const;
    
    // 检查是否已打开
    bool isOpen() const;
    
    // 获取宝箱类型名称
    std::string getChestTypeName() const;
};

// 宝箱管理器类
class ChestManager {
private:
    // 使用 unordered_map 存储宝箱，键是节点ID，值是宝箱对象
    std::unordered_map<std::string, TreasureChest> chests_;

public:
    // 创建宝箱
    void createChest(const std::string& nodeId, ChestType type, LootFactory& factory);
    
    // 创建随机宝箱
    void createRandomChest(const std::string& nodeId, LootFactory& factory);
    
    // 检查节点是否有宝箱
    bool hasChest(const std::string& nodeId) const;
    
    // 打开宝箱
    Loot openChest(const std::string& nodeId, LootFactory& factory);
    
    // 获取宝箱
    TreasureChest* getChest(const std::string& nodeId);
    
    // 重置所有宝箱
    void resetAllChests(LootFactory& factory);
};

} // namespace tce

#endif // TREASURE_CHEST_HPP
