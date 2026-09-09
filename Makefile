# Zilla - build rules.
#
#   make            builds build/zilla (needs GLFW 3 and the Vulkan loader)
#   make preview    builds build/zilla-preview and renders PNG screenshots - no GPU needed
#   make shaders    recompiles shaders/*.glsl (needs glslangValidator from the Vulkan SDK)
#   make run        builds and runs the game
#
# Dependencies: a C++17 compiler, GLFW 3 and the Vulkan runtime (loader).

CXX      ?= g++
CC       ?= cc
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CFLAGS   ?= -std=c99 -O2

INCLUDES := -Isrc -Ithirdparty -Ithirdparty/vulkan/include

BUILD := build

CXX_SOURCES := \
	src/main.cpp \
	src/core/log.cpp \
	src/gfx/draw_list.cpp \
	src/gfx/font.cpp \
	src/gfx/vulkan.cpp \
	src/platform/window.cpp \
	src/ui/ui.cpp \
	src/game/game.cpp

C_SOURCES := thirdparty/volk/volk.c

PREVIEW_SOURCES := \
	tools/preview.cpp \
	src/core/log.cpp \
	src/gfx/draw_list.cpp \
	src/gfx/font.cpp \
	src/ui/ui.cpp \
	src/game/game.cpp

OBJECTS         := $(CXX_SOURCES:%.cpp=$(BUILD)/%.o) $(C_SOURCES:%.c=$(BUILD)/%.o)
PREVIEW_OBJECTS := $(PREVIEW_SOURCES:%.cpp=$(BUILD)/%.o)

TARGET  := $(BUILD)/zilla
PREVIEW := $(BUILD)/zilla-preview

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	GLFW_LIBS ?= $(shell pkg-config --libs glfw3 2>/dev/null || echo -lglfw)
	LDLIBS := $(GLFW_LIBS) -ldl -lpthread -lm
endif
ifeq ($(UNAME_S),Darwin)
	GLFW_LIBS ?= $(shell pkg-config --libs glfw3 2>/dev/null || echo -lglfw)
	LDLIBS := $(GLFW_LIBS) -ldl -lpthread -lm -framework Cocoa -framework IOKit -framework CoreVideo
endif
ifneq (,$(filter MINGW% MSYS% CYGWIN%,$(UNAME_S)))
	GLFW_LIBS ?= -lglfw3
	LDLIBS := $(GLFW_LIBS) -lgdi32
endif

.PHONY: all preview shaders run clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) -o $@ $(LDLIBS)

$(PREVIEW): $(PREVIEW_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(PREVIEW_OBJECTS) -o $@ -lm

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

preview: $(PREVIEW)
	@mkdir -p $(BUILD)
	$(PREVIEW) assets $(BUILD)

shaders:
	@if command -v glslangValidator >/dev/null 2>&1; then \
		glslangValidator -V shaders/quad.vert.glsl -o shaders/spv/quad.vert.spv && \
		glslangValidator -V shaders/quad.frag.glsl -o shaders/spv/quad.frag.spv && \
		python3 tools/embed_shaders.py src/gfx/embedded_shaders.h shaders/spv/quad.vert.spv shaders/spv/quad.frag.spv; \
	else \
		echo "glslangValidator not found - keeping the committed SPIR-V modules"; \
	fi

run: $(TARGET)
	ZILLA_ASSETS=assets ./$(TARGET)

clean:
	rm -rf $(BUILD)
