/**
 * 事件 / 商店 / 休息 UI 所需数据结构
 * 由主流程或 D 模块填充，供 EventShopRestUI 绘制与轮询选择结果
 */
#pragma once

#include "Common/Types.hpp"
#include <string>
#include <vector>

namespace tce {

/** 当前显示的界面类型 */
enum class EventShopRestScreen {
    None,   // 不显示覆盖层
    Event,  // 事件：标题、描述、选项列表
    Shop,   // 商店：可购买的牌、可删除的牌
    Rest,   // 休息：回血 or 升级一张牌
};

/** 事件界面数据 */
struct EventDisplayData {
    std::wstring title;
    std::wstring description;
    std::vector<std::wstring> optionTexts;
    // 每个选项对应的“选后效果预览”，会在选项下方以红字单独显示
    std::vector<std::wstring> optionEffectTexts;
    std::string imagePath;  // 可选，插图路径，空则显示占位
};

/** 商店中单张可购买牌 */
struct ShopCardOffer {
    CardId id;
    std::wstring name;
    int price = 0;
};

/** 商店遗物槽位 */
struct ShopRelicOffer {
    std::string id;
    std::wstring name;
    int price = 0;
};

/** 商店药剂槽位 */
struct ShopPotionOffer {
    std::string id;
    std::wstring name;
    int price = 0;
};

/** 牌组中一张牌（用于商店删牌 / 休息升级时展示） */
struct MasterDeckCardDisplay {
    InstanceId instanceId = 0;
    CardId cardId;  // 用于复用战斗卡面渲染（可为空，表示仅有 cardName）
    std::wstring cardName;
};

/** 商店界面数据 */
struct ShopDisplayData {
    std::vector<ShopCardOffer> forSale;              // 第一行：通常 5 张
    /** 第二行左侧：对齐上排第 1、2 列的无色/特殊牌（仿 StS 下排两张） */
    std::vector<ShopCardOffer> colorlessForSale;
    std::vector<ShopRelicOffer> relicsForSale;       // 第二行中上：3 遗物
    std::vector<ShopPotionOffer> potionsForSale;     // 第二行中下：3 灵液
    std::vector<MasterDeckCardDisplay> deckForRemove;

    int playerGold = 0;
    int playerCurrentHp = 0;
    int playerMaxHp = 0;
    int potionSlotsMax = 3;
    int potionSlotsUsed = 0;
    std::wstring chapterLine;
    /** 顶栏左侧称谓，空则用「溯源者」 */
    std::wstring playerTitle;

    /** 净简服务：支付后可从牌组选一张移除；本趟商店用过后售罄 */
    int removeServicePrice = 75;
    bool removeServicePaid = false;
    bool removeServiceSoldOut = false;
};

/** 休息界面数据 */
struct RestDisplayData {
    int healAmount = 0;
    std::vector<MasterDeckCardDisplay> deckForUpgrade;  // 当前牌组，供玩家选一张升级
};

} // namespace tce
