#pragma once

#include <SFML/Graphics.hpp>
#include <optional>

namespace tce {

/** 可选职业（目前：铁甲战士、静默猎手） */
enum class CharacterClass { Ironclad, Silent };

/**
 * 职业选择界面：点击角色卡选中，再点“确认”返回选择；
 * - 返回 std::nullopt 表示取消（Esc / 关闭窗口）
 */
std::optional<CharacterClass> runCharacterSelectScreen(sf::RenderWindow& window);

} // namespace tce

