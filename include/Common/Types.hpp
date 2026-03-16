/**
 * 公共标识类型与枚举（与 E 约定一致）
 * B、C、E 共用
 */
#pragma once

#include <string>
#include <cstdint>

namespace tce {

    using CardId = std::string;
    using MonsterId = std::string;
    using StatusId = std::string;
    using PotionId = std::string;
    using RelicId = std::string;

    // 牌组内唯一 id，用于区分多张同 CardId 的牌
    using InstanceId = int;


    // namespace tce
    enum class Stance {
        Neutral,
        Calm,
        Wrath,
        Divinity,
    };
}