# Makefile for Code Runner + g++ (MinGW) + SFML
# 需要 MinGW 编译的 SFML，不能用 MSVC 版。可从 SFML 官网下载 MinGW 预编译包，或自行编译。
# 若 SFML 在别处，可改 SFML_DIR。C 盘为 MinGW 版，D 盘为 MSVC 版（需用 VS 编译）
# 示例: mingw32-make SFML_DIR=D:/SFML-MinGW

CXX       = g++
SFML_DIR ?= C:/SFML-3.0.2
CXXFLAGS  = -std=c++17 -Wall -I include -I $(SFML_DIR)/include -D_DEBUG -DCONSOLE -DTEST_BATTLE_UI
LDFLAGS   = -L $(SFML_DIR)/lib -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio

SRCS = src/main.cpp \
       src/DataLayer/JsonParser.cpp \
       src/DataLayer/DataLayer.cpp \
       src/CardSystem/CardSystem.cpp \
       src/CardSystem/DeckViewCollection.cpp \
       src/BattleEngine/BattleEngine.cpp \
       src/BattleEngine/BattleUI.cpp \
       src/BattleEngine/BattleUISnapshotAdapter.cpp \
       src/BattleEngine/MonsterBehaviors.cpp \
       src/BattleCoreRefactor/PotionEffects.cpp \
       src/BattleCoreRefactor/RelicModifiers.cpp \
       src/BattleCoreRefactor/StatusModifiers.cpp \
       src/BattleCoreRefactor/BattleCoreRefactorSnapshotAdapter.cpp \
       src/Effects/CardEffects.cpp \
       src/EventEngine/EventEngine.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = build/Tracer_Civilization_Environment.exe

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@if not exist build mkdir build
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo Copying SFML DLLs...
	@xcopy /Y /D "$(SFML_DIR)\bin\*.*" build\ 2>nul || echo "请手动将 SFML bin 下的 DLL 复制到 build 目录"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	del /Q $(OBJS) 2>nul
	del /Q $(TARGET) 2>nul
