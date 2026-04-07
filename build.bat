@echo off
REM Code Runner + g++ (MinGW) + SFML 构建脚本
REM 需要 MinGW 版 SFML（.a 库）。当前 SFML-3.0.2 为 MSVC 版，g++ 无法链接。
REM 若已有 MinGW 版 SFML，设置环境变量: set SFML_DIR=你的MinGW版SFML路径

setlocal
cd /d "%~dp0"

if not defined SFML_DIR set SFML_DIR=C:\SFML-3.0.2

if not exist build mkdir build

g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/main.cpp -o build/main.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/Common/ImagePath.cpp -o build/ImagePath.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/DataLayer/JsonParser.cpp -o build/JsonParser.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/DataLayer/DataLayer.cpp -o build/DataLayer.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/CardSystem/CardSystem.cpp -o build/CardSystem.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/CardSystem/DeckViewCollection.cpp -o build/DeckViewCollection.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/BattleEngine/BattleEngine.cpp -o build/BattleEngine.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/BattleEngine/BattleUI.cpp -o build/BattleUI.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/BattleEngine/BattleUISnapshotAdapter.cpp -o build/BattleUISnapshotAdapter.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/BattleEngine/MonsterBehaviors.cpp -o build/MonsterBehaviors.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/BattleCoreRefactor/PotionEffects.cpp -o build/PotionEffects.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/BattleCoreRefactor/RelicModifiers.cpp -o build/RelicModifiers.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/BattleCoreRefactor/StatusModifiers.cpp -o build/StatusModifiers.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/BattleCoreRefactor/BattleCoreRefactorSnapshotAdapter.cpp -o build/BattleCoreRefactorSnapshotAdapter.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/Effects/CardEffects.cpp -o build/CardEffects.o
g++ -std=c++17 -Wall -I include -I %SFML_DIR%/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI ^
    -c src/EventEngine/EventEngine.cpp -o build/EventEngine.o

g++ -o build/Tracer_Civilization_Environment.exe ^
    build/main.o build/ImagePath.o build/JsonParser.o build/DataLayer.o build/CardSystem.o build/DeckViewCollection.o ^
    build/BattleEngine.o build/BattleUI.o build/BattleUISnapshotAdapter.o build/MonsterBehaviors.o ^
    build/PotionEffects.o build/RelicModifiers.o build/StatusModifiers.o build/BattleCoreRefactorSnapshotAdapter.o ^
    build/CardEffects.o build/EventEngine.o ^
    -L %SFML_DIR%/lib -lsfml-graphics -lsfml-window -lsfml-system

if %ERRORLEVEL% neq 0 exit /b 1

xcopy /Y /D "%SFML_DIR%\bin\*.*" build\ 2>nul
echo 构建完成。运行: build\Tracer_Civilization_Environment.exe
