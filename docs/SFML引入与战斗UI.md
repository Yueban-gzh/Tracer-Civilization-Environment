# SFML 引入步骤与战斗调试 UI

用于构筑**战斗系统独享的调试界面**，先在本机引入 SFML 库，再在项目中配置并打开一个窗口。

---

## 当前工程已配置的 SFML 路径

- **SFML 目录**：`D:\C++Project\SFML-3.0.2`（在 vcxproj 中通过宏 `SFML_DIR` 引用，可统一修改）。
- **已配置**：Debug|x64、Release|x64 的**包含目录**、**库目录**、**附加依赖项**；生成后自动把 `$(SFML_DIR)\bin\*.*` 复制到 exe 输出目录。
- **请确认你本机目录结构**：
  - `D:\C++Project\SFML-3.0.2\include`（头文件）
  - `D:\C++Project\SFML-3.0.2\lib`（.lib，Debug 用带 `-d` 的库名）
  - `D:\C++Project\SFML-3.0.2\bin`（.dll，运行前会复制到 exe 同目录）
- 若你的 lib 在子目录（如 `lib\x64`），请在项目属性 → 链接器 → 附加库目录中改为 `$(SFML_DIR)\lib\x64`。

---

## 一、引入 SFML 的两种方式（备用参考）

### 方式一：使用 vcpkg（推荐，自动处理依赖）

1. **安装 vcpkg**（若尚未安装）
   - 打开 PowerShell 或 CMD，选一个**路径无中文、无空格**的目录，例如 `D:\Dev`：
   ```bat
   cd /d D:\Dev
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat -disableMetrics
   ```
   - 可选：执行 `.\vcpkg integrate install`，让本机 Visual Studio 能通过“清单模式”或集成脚本使用 vcpkg。

2. **安装 SFML（x64，与当前工程一致）**
   ```bat
   .\vcpkg install sfml:x64-windows
   ```
   - 若还需要 Win32 调试，可再执行：`.\vcpkg install sfml:x86-windows`。

3. **在工程中启用 vcpkg**
   - 在**解决方案资源管理器**里右键项目 → **属性**。
   - **配置**选 **Debug | x64**（或 All Configurations），**平台**选 **x64**。
   - **VC++ 目录**：
     - **包含目录** 添加（把 `D:\Dev` 换成你的 vcpkg 根目录）：
       ```
       D:\Dev\vcpkg\installed\x64-windows\include
       ```
     - **库目录** 添加：
       ```
       D:\Dev\vcpkg\installed\x64-windows\lib
       ```
   - **链接器 → 输入 → 附加依赖项**：
     - **Debug**：  
       `sfml-graphics-d.lib;sfml-window-d.lib;sfml-system-d.lib`
     - **Release**：  
       `sfml-graphics.lib;sfml-window.lib;sfml-system.lib`
   - **C/C++ → 预处理器 → 预处理器定义**（仅 Debug）：  
     添加 `SFML_STATIC` 仅当你要用静态链接；用 vcpkg 默认动态链接时可**不**加 `SFML_STATIC`，保持默认即可。
   - 若使用**动态链接**（vcpkg 默认）：  
     把 vcpkg 里的 DLL 拷到 exe 同目录，或把 `D:\Dev\vcpkg\installed\x64-windows\bin` 加入系统 PATH。  
     vcpkg 的 DLL 在 `installed\x64-windows\bin` 下。

4. **Release | x64** 再按同样方式配一遍（库目录相同，附加依赖项用无 `-d` 的 lib）。

---

### 方式二：使用官网预编译包

1. **下载**
   - 打开 [SFML 2.6 下载页](https://www.sfml-dev.org/download/sfml/2.6/)。
   - 选择 **Visual C++ 17 (2022) - 64-bit**（与 VS2022、x64 对应），解压到路径**无中文**的目录，例如 `D:\Libs\SFML-2.6`.

2. **项目属性（以 Debug | x64 为例）**
   - **C/C++ → 常规 → 附加包含目录**：  
     `D:\Libs\SFML-2.6\include`
   - **链接器 → 常规 → 附加库目录**：  
     `D:\Libs\SFML-2.6\lib`
   - **链接器 → 输入 → 附加依赖项**：
     - Debug：`sfml-graphics-d.lib;sfml-window-d.lib;sfml-system-d.lib`
     - Release：`sfml-graphics.lib;sfml-window.lib;sfml-system.lib`

3. **运行时有 DLL**
   - 把 `D:\Libs\SFML-2.6\bin` 下对应配置的 DLL（如 Debug 的 `*-d.dll`）复制到你的 exe 输出目录（如 `x64\Debug`），  
     或在 **调试 → 环境** 里添加：  
     `PATH=D:\Libs\SFML-2.6\bin;%PATH%`  
   - 注意：Debug 用带 `-d` 的 DLL，Release 用不带 `-d` 的。

4. **Release | x64** 同样配一遍，库用无 `-d` 的 lib，DLL 用无 `-d` 的。

---

## 二、验证：最小窗口

配置好后，用下面代码替换或临时放在 `main.cpp` 里，编译运行应弹出一个 800×600 的窗口，用于确认 SFML 已正确引入：

```cpp
#include <SFML/Graphics.hpp>

int main() {
    sf::RenderWindow window(sf::VideoMode(800, 600), "Battle Debug");
    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed)
                window.close();
        }
        window.clear(sf::Color(30, 30, 40));
        window.display();
    }
    return 0;
}
```

- 若出现“找不到 SFML/Graphics.hpp”或链接错误，请按上面步骤再检查**包含目录、库目录、附加依赖项**是否与配置（Debug/Release、x64）一致。
- 若运行时报“找不到 xxx.dll”，请检查 **bin** 是否在 PATH 或 exe 同目录。

---

## 三、下一步：战斗调试 UI

SFML 引入并验证通过后，可以：

1. 在 `src/` 下新增**战斗调试界面**模块（如 `BattleDebugUI.cpp/.hpp`），只负责：
   - 从 `BattleEngine::get_battle_state()` 取 `BattleStateSnapshot`；
   - 用 SFML 绘制：玩家血量/能量/格挡、手牌数量、怪物列表、药水/遗物数量等，便于看数据变化。
2. `main.cpp` 中：
   - 创建 `CardSystem`、`BattleEngine`，调用 `start_battle` 注入测试用怪物与牌组；
   - 创建 SFML 窗口与上述战斗 UI，每帧调用 `get_battle_state()` 并刷新绘制；
   - 可加简单按键：如空格结束回合、数字键出牌等，方便单步调试。

如果你已经按上面步骤配好 SFML 并成功打开最小窗口，可以说一下当前是 vcpkg 还是预编译、以及输出目录结构，我可以按你现有工程结构写出 `BattleDebugUI` 的接口和第一版绘制代码（仅 SFML + BattleStateSnapshot）。
