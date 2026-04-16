#include "Treasure/TreasureChest.hpp"
#include <cstdlib>
#include <algorithm>
#include <ctime>
#include <iostream>
#include <tuple>   // 用于 std::piecewise_construct, std::forward_as_tuple
#include <utility> // 用于 std::ref

namespace tce {

// --- LootFactory 实现 ---

LootFactory::LootFactory() {
    // 初始化随机数生成器
    std::random_device rd;
    rng_ = std::mt19937(rd());
    
    // 初始化宝箱类型配置
    chestConfigs_ = {
        {ChestType::Small, 0.50f, 0.75f, 0.25f, 0.00f, 23, 27},
        {ChestType::Medium, 0.33f, 0.35f, 0.50f, 0.15f, 45, 55},
        {ChestType::Large, 0.17f, 0.00f, 0.75f, 0.25f, 68, 82}
    };
    
    // 初始化遗物数据库
    initializeRelicDatabase();
}

void LootFactory::initializeRelicDatabase() {
    // 普通遗物
    commonRelics_ = {
        {"relic_001", "生锈的剑", "一把生锈的旧剑，勉强能用。", 10},
        {"relic_002", "破损的盾牌", "布满划痕的木盾。", 15},
        {"relic_003", "少量金币", "几枚铜币。", 5},
        {"relic_004", "草药", "一些普通的草药。", 8},
        {"relic_005", "铁钥匙", "一把普通的铁钥匙。", 12},
        {"relic_006", "皮甲", "一件破旧的皮甲。", 18},
        {"relic_007", "铜戒指", "一枚普通的铜戒指。", 20},
        {"relic_008", "面包", "一块普通的面包。", 6}
    };
    
    // 罕见遗物
    uncommonRelics_ = {
        {"relic_009", "精钢长剑", "锋利的钢剑，闪烁着寒光。", 50},
        {"relic_010", "附魔护符", "散发着微弱魔力的护符。", 60},
        {"relic_011", "治疗灵液", "红色的液体，喝了能恢复体力。", 40},
        {"relic_012", "银盾", "闪闪发光的银盾。", 45},
        {"relic_013", "魔法卷轴", "记载着初级魔法的卷轴。", 55},
        {"relic_014", "银钥匙", "一把精致的银钥匙。", 35},
        {"relic_015", "链甲", "一件坚固的链甲。", 65},
        {"relic_016", "金戒指", "一枚华丽的金戒指。", 70},
        {"relic_017", "魔法灵液", "蓝色的液体，喝了能恢复魔力。", 45},
        {"relic_018", "宝石", "一颗闪亮的宝石。", 80}
    };
    
    // 稀有遗物
    rareRelics_ = {
        {"relic_019", "传说之剑", "古代英雄使用的宝剑。", 200},
        {"relic_020", "龙王之盾", "坚不可摧的神器。", 250},
        {"relic_021", "复活石", "拥有起死回生之力的石头。", 500},
        {"relic_022", "圣者之杖", "充满神圣力量的法杖。", 300},
        {"relic_023", "凤凰羽", "凤凰的羽毛，蕴含着生命之力。", 350},
        {"relic_024", "龙鳞甲", "用龙鳞制作的盔甲。", 400},
        {"relic_025", "星辰项链", "镶嵌着星辰碎片的项链。", 450},
        {"relic_026", "时间沙漏", "能够扭曲时间的神器。", 600},
        {"relic_027", "空间戒指", "能够存储物品的戒指。", 550},
        {"relic_028", "命运之轮", "能够改变命运的神器。", 700}
    };
}

ChestType LootFactory::createRandomChestType() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float randVal = dist(rng_);
    
    float cumulativeProbability = 0.0f;
    for (const auto& config : chestConfigs_) {
        cumulativeProbability += config.probability;
        if (randVal <= cumulativeProbability) {
            return config.type;
        }
    }
    
    return ChestType::Small; // 默认返回小型宝箱
}

Loot LootFactory::generateLoot(ChestType type) {
    const auto& config = getChestConfig(type);
    Loot loot;
    
    // 生成遗物
    std::uniform_real_distribution<float> relicDist(0.0f, 1.0f);
    float relicRand = relicDist(rng_);
    float cumulativeWeight = 0.0f;
    
    if (relicRand <= config.commonRelicWeight) {
        // 普通遗物
        std::uniform_int_distribution<int> commonDist(0, commonRelics_.size() - 1);
        int index = commonDist(rng_);
        loot.relic = commonRelics_[index];
    } else if (relicRand <= config.commonRelicWeight + config.uncommonRelicWeight) {
        // 罕见遗物
        std::uniform_int_distribution<int> uncommonDist(0, uncommonRelics_.size() - 1);
        int index = uncommonDist(rng_);
        loot.relic = uncommonRelics_[index];
    } else {
        // 稀有遗物
        std::uniform_int_distribution<int> rareDist(0, rareRelics_.size() - 1);
        int index = rareDist(rng_);
        loot.relic = rareRelics_[index];
    }
    
    // 50%概率掉落金币
    std::uniform_real_distribution<float> goldDist(0.0f, 1.0f);
    loot.hasGold = goldDist(rng_) <= 0.5f;
    if (loot.hasGold) {
        std::uniform_int_distribution<int> goldAmountDist(config.minGold, config.maxGold);
        loot.gold = goldAmountDist(rng_);
    } else {
        loot.gold = 0;
    }
    
    return loot;
}

const ChestTypeConfig& LootFactory::getChestConfig(ChestType type) const {
    for (const auto& config : chestConfigs_) {
        if (config.type == type) {
            return config;
        }
    }
    // 默认返回小型宝箱配置
    return chestConfigs_[0];
}

void LootFactory::addRelic(const Relic& relic, bool isCommon, bool isUncommon, bool isRare) {
    if (isCommon) {
        commonRelics_.push_back(relic);
    }
    if (isUncommon) {
        uncommonRelics_.push_back(relic);
    }
    if (isRare) {
        rareRelics_.push_back(relic);
    }
}

// --- TreasureChest 实现 ---

TreasureChest::TreasureChest(ChestType type, LootFactory& factory) 
    : type_(type), state_(ChestState::Closed), isOpened_(false) {
    // 初始化时不生成战利品，只有在打开时生成
}

Loot TreasureChest::open(LootFactory& factory) {
    if (!isOpened_) {
        loot_ = factory.generateLoot(type_);
        state_ = ChestState::Open;
        isOpened_ = true;
        
        // 输出开箱信息
        std::cout << "\n[宝箱] 你打开了一个" << getChestTypeName() << "！" << std::endl;
        std::cout << "  获得遗物: " << loot_.relic.name << std::endl;
        std::cout << "  描述: " << loot_.relic.description << std::endl;
        if (loot_.hasGold) {
            std::cout << "  获得金币: " << loot_.gold << std::endl;
        }
        std::cout << std::endl;
    } else {
        std::cout << "[宝箱] 这个宝箱已经被打开过了。" << std::endl;
    }
    return loot_;
}

ChestType TreasureChest::getChestType() const {
    return type_;
}

ChestState TreasureChest::getState() const {
    return state_;
}

bool TreasureChest::isOpen() const {
    return isOpened_;
}

std::string TreasureChest::getChestTypeName() const {
    switch (type_) {
        case ChestType::Small:
            return "小型宝箱";
        case ChestType::Medium:
            return "中型宝箱";
        case ChestType::Large:
            return "大型宝箱";
        default:
            return "未知宝箱";
    }
}

// --- ChestManager 实现 ---

// 在 TreasureChest.cpp 中修改 createChest 函数

void ChestManager::createChest(const std::string& nodeId, ChestType type, LootFactory& factory) {
    // 使用 piecewise_construct 来分别构造 key 和 value
    chests_.emplace(
        std::piecewise_construct,                 // 标记：我要分段构造
        std::forward_as_tuple(nodeId),            // 第一部分：构造 Key (string)
        std::forward_as_tuple(type, std::ref(factory)) // 第二部分：构造 Value (TreasureChest)
    );
}

void ChestManager::createRandomChest(const std::string& nodeId, LootFactory& factory) {
    ChestType type = factory.createRandomChestType();
    createChest(nodeId, type, factory);
}

bool ChestManager::hasChest(const std::string& nodeId) const {
    return chests_.find(nodeId) != chests_.end();
}

Loot ChestManager::openChest(const std::string& nodeId, LootFactory& factory) {
    auto it = chests_.find(nodeId);
    if (it != chests_.end()) {
        return it->second.open(factory);
    }
    // 返回空战利品
    return Loot{};
}

TreasureChest* ChestManager::getChest(const std::string& nodeId) {
    auto it = chests_.find(nodeId);
    if (it != chests_.end()) {
        return &(it->second);
    }
    return nullptr;
}

void ChestManager::resetAllChests(LootFactory& factory) {
    for (auto& pair : chests_) {
        // 重新创建宝箱
        pair.second = TreasureChest(pair.second.getChestType(), factory);
    }
}

} // namespace tce
