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

/** 牌组中一张牌（用于商店删牌 / 休息升级时展示） */
struct MasterDeckCardDisplay {
    InstanceId instanceId = 0;
    std::wstring cardName;
};

/** 商店界面数据 */
struct ShopDisplayData {
    std::vector<ShopCardOffer> forSale;
    std::vector<MasterDeckCardDisplay> deckForRemove;  // 当前牌组，供玩家选一张删除
    int playerGold = 0;
};

/** 休息界面数据 */
struct RestDisplayData {
    int healAmount = 0;
    std::vector<MasterDeckCardDisplay> deckForUpgrade;  // 当前牌组，供玩家选一张升级
};

} // namespace tce
