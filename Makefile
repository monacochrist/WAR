CC := gcc
DEBUG ?= 0
VERBOSE ?= 0

WL_SHM ?= 0
DMABUF ?= 0

PIPEWIRE_CFLAGS := $(shell pkg-config --cflags libpipewire-0.3)
PIPEWIRE_LIBS   := $(shell pkg-config --libs libpipewire-0.3)

ifeq ($(WL_SHM),1)
	DMABUF := 0
else ifeq ($(DMABUF),1)
	WL_SHM := 0
else
	DMABUF := 1
endif

ifeq ($(VERBOSE), 1)
    Q :=
else
    Q := @
    MAKEFLAGS += --no-print-directory
endif

ifeq ($(DEBUG), 1)
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O3 -g -march=native -std=c99 -MMD -I src -I include -I /usr/include/libdrm -I /usr/include/freetype2 $(PIPEWIRE_CFLAGS)
else ifeq ($(DEBUG), 2)
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O0 -g -march=native -std=c99 -MMD -DDEBUG -I src -I include -I /usr/include/libdrm -I /usr/include/freetype2 $(PIPEWIRE_CFLAGS) 
else
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O3 -march=native -std=c99 -MMD -DNDEBUG -I src -I include -I /usr/include/libdrm -I /usr/include/freetype2 $(PIPEWIRE_CFLAGS)
endif

CFLAGS += -DWL_SHM=$(WL_SHM)
CFLAGS += -DDMABUF=$(DMABUF)

LDFLAGS := -lvulkan -ldrm -lm -lluajit-5.1 -lxkbcommon -lasound -lpthread -lfreetype $(PIPEWIRE_LIBS)

SRC_DIR := src
BUILD_DIR := build
TARGET := WAR

GLSLC := glslangValidator
SHADER_SRC_DIR := src/glsl
SHADER_BUILD_DIR := $(BUILD_DIR)/spv
NSGT_VERT_SHADER_SRC := $(SHADER_SRC_DIR)/war_nsgt_vertex.glsl
NSGT_FRAG_SHADER_SRC := $(SHADER_SRC_DIR)/war_nsgt_fragment.glsl
NSGT_VERT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_nsgt_vertex.spv
NSGT_FRAG_SHADER_SPV := $(SHADER_BUILD_DIR)/war_nsgt_fragment.spv
NSGT_COMPUTE_NSGT_SHADER_SRC := $(SHADER_SRC_DIR)/war_nsgt_compute_nsgt.glsl
NSGT_COMPUTE_NSGT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_nsgt_compute_nsgt.spv
NSGT_COMPUTE_MAGNITUDE_SHADER_SRC := $(SHADER_SRC_DIR)/war_nsgt_compute_magnitude.glsl
NSGT_COMPUTE_MAGNITUDE_SHADER_SPV := $(SHADER_BUILD_DIR)/war_nsgt_compute_magnitude.spv
NSGT_COMPUTE_TRANSIENT_SHADER_SRC := $(SHADER_SRC_DIR)/war_nsgt_compute_transient.glsl
NSGT_COMPUTE_TRANSIENT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_nsgt_compute_transient.spv
NSGT_COMPUTE_IMAGE_SHADER_SRC := $(SHADER_SRC_DIR)/war_nsgt_compute_image.glsl
NSGT_COMPUTE_IMAGE_SHADER_SPV := $(SHADER_BUILD_DIR)/war_nsgt_compute_image.spv
NSGT_COMPUTE_WAV_SHADER_SRC := $(SHADER_SRC_DIR)/war_nsgt_compute_wav.glsl
NSGT_COMPUTE_WAV_SHADER_SPV := $(SHADER_BUILD_DIR)/war_nsgt_compute_wav.spv
NEW_VULKAN_VERTEX_CURSOR_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_vertex_cursor.glsl
NEW_VULKAN_VERTEX_CURSOR_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_vertex_cursor.spv
NEW_VULKAN_FRAGMENT_CURSOR_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_fragment_cursor.glsl
NEW_VULKAN_FRAGMENT_CURSOR_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_fragment_cursor.spv
NEW_VULKAN_VERTEX_NOTE_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_vertex_note.glsl
NEW_VULKAN_VERTEX_NOTE_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_vertex_note.spv
NEW_VULKAN_FRAGMENT_NOTE_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_fragment_note.glsl
NEW_VULKAN_FRAGMENT_NOTE_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_fragment_note.spv
NEW_VULKAN_VERTEX_TEXT_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_vertex_text.glsl
NEW_VULKAN_VERTEX_TEXT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_vertex_text.spv
NEW_VULKAN_FRAGMENT_TEXT_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_fragment_text.glsl
NEW_VULKAN_FRAGMENT_TEXT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_fragment_text.spv
NEW_VULKAN_VERTEX_LINE_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_vertex_line.glsl
NEW_VULKAN_VERTEX_LINE_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_vertex_line.spv
NEW_VULKAN_FRAGMENT_LINE_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_fragment_line.glsl
NEW_VULKAN_FRAGMENT_LINE_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_fragment_line.spv
NEW_VULKAN_VERTEX_HUD_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_vertex_hud.glsl
NEW_VULKAN_VERTEX_HUD_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_vertex_hud.spv
NEW_VULKAN_FRAGMENT_HUD_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_fragment_hud.glsl
NEW_VULKAN_FRAGMENT_HUD_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_fragment_hud.spv
NEW_VULKAN_VERTEX_HUD_CURSOR_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_vertex_hud_cursor.glsl
NEW_VULKAN_VERTEX_HUD_CURSOR_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_vertex_hud_cursor.spv
NEW_VULKAN_FRAGMENT_HUD_CURSOR_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_fragment_hud_cursor.glsl
NEW_VULKAN_FRAGMENT_HUD_CURSOR_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_fragment_hud_cursor.spv
NEW_VULKAN_VERTEX_HUD_LINE_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_vertex_hud_line.glsl
NEW_VULKAN_VERTEX_HUD_LINE_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_vertex_hud_line.spv
NEW_VULKAN_FRAGMENT_HUD_LINE_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_fragment_hud_line.glsl
NEW_VULKAN_FRAGMENT_HUD_LINE_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_fragment_hud_line.spv
NEW_VULKAN_VERTEX_HUD_TEXT_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_vertex_hud_text.glsl
NEW_VULKAN_VERTEX_HUD_TEXT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_vertex_hud_text.spv
NEW_VULKAN_FRAGMENT_HUD_TEXT_SHADER_SRC := $(SHADER_SRC_DIR)/war_new_vulkan_fragment_hud_text.glsl
NEW_VULKAN_FRAGMENT_HUD_TEXT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_new_vulkan_fragment_hud_text.spv
SPV_BUILD_FILES := \
	$(NSGT_VERT_SHADER_SPV) \
	$(NSGT_FRAG_SHADER_SPV) \
	$(NSGT_COMPUTE_NSGT_SHADER_SPV) \
	$(NSGT_COMPUTE_MAGNITUDE_SHADER_SPV) \
	$(NSGT_COMPUTE_TRANSIENT_SHADER_SPV) \
	$(NSGT_COMPUTE_IMAGE_SHADER_SPV) \
	$(NSGT_COMPUTE_WAV_SHADER_SPV) \
	$(NEW_VULKAN_FRAGMENT_CURSOR_SHADER_SPV) \
	$(NEW_VULKAN_FRAGMENT_NOTE_SHADER_SPV) \
	$(NEW_VULKAN_FRAGMENT_TEXT_SHADER_SPV) \
	$(NEW_VULKAN_FRAGMENT_LINE_SHADER_SPV) \
	$(NEW_VULKAN_FRAGMENT_HUD_SHADER_SPV) \
	$(NEW_VULKAN_FRAGMENT_HUD_CURSOR_SHADER_SPV) \
	$(NEW_VULKAN_FRAGMENT_HUD_LINE_SHADER_SPV) \
	$(NEW_VULKAN_FRAGMENT_HUD_TEXT_SHADER_SPV) \
	$(NEW_VULKAN_VERTEX_CURSOR_SHADER_SPV) \
	$(NEW_VULKAN_VERTEX_NOTE_SHADER_SPV) \
	$(NEW_VULKAN_VERTEX_TEXT_SHADER_SPV) \
	$(NEW_VULKAN_VERTEX_LINE_SHADER_SPV) \
	$(NEW_VULKAN_VERTEX_HUD_SHADER_SPV) \
	$(NEW_VULKAN_VERTEX_HUD_CURSOR_SHADER_SPV) \
	$(NEW_VULKAN_VERTEX_HUD_LINE_SHADER_SPV) \
	$(NEW_VULKAN_VERTEX_HUD_TEXT_SHADER_SPV) \

H_BUILD_DIR := $(SRC_DIR)/h
BUILD_KEYMAP_FUNCTIONS_H := $(SRC_DIR)/lua/war_build_keymap_functions.lua
H_BUILD_FILES := \
	$(BUILD_KEYMAP_FUNCTIONS_H) \

SRC := $(shell find $(SRC_DIR) -type f -name '*.c')

UNITY_C := $(SRC_DIR)/war_main.c
UNITY_O := $(BUILD_DIR)/war_main.o
DEP := $(UNITY_O:.o=.d)

.PHONY: all clean gcc_check

all: $(H_BUILD_FILES) $(SPV_BUILD_FILES) $(TARGET)

$(SHADER_BUILD_DIR):
	$(Q)mkdir -p $(SHADER_BUILD_DIR)

$(BUILD_DIR):
	$(Q)mkdir -p $(BUILD_DIR)

$(H_BUILD_DIR): $(BUILD_DIR)
	$(Q)mkdir -p $(H_BUILD_DIR)

$(BUILD_KEYMAP_FUNCTIONS_H): $(H_BUILD_DIR)
	$(Q)lua $(BUILD_KEYMAP_FUNCTIONS_H)

$(NSGT_VERT_SHADER_SPV): $(NSGT_VERT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@
$(NSGT_FRAG_SHADER_SPV): $(NSGT_FRAG_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
$(NSGT_COMPUTE_MAGNITUDE_SHADER_SPV): $(NSGT_COMPUTE_MAGNITUDE_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S comp $< -o $@
$(NSGT_COMPUTE_IMAGE_SHADER_SPV): $(NSGT_COMPUTE_IMAGE_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S comp $< -o $@
$(NSGT_COMPUTE_WAV_SHADER_SPV): $(NSGT_COMPUTE_WAV_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S comp $< -o $@
$(NSGT_COMPUTE_TRANSIENT_SHADER_SPV): $(NSGT_COMPUTE_TRANSIENT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S comp $< -o $@
$(NSGT_COMPUTE_NSGT_SHADER_SPV): $(NSGT_COMPUTE_NSGT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S comp $< -o $@
$(NEW_VULKAN_FRAGMENT_CURSOR_SHADER_SPV): $(NEW_VULKAN_FRAGMENT_CURSOR_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
$(NEW_VULKAN_FRAGMENT_NOTE_SHADER_SPV): $(NEW_VULKAN_FRAGMENT_NOTE_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
$(NEW_VULKAN_FRAGMENT_TEXT_SHADER_SPV): $(NEW_VULKAN_FRAGMENT_TEXT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
$(NEW_VULKAN_FRAGMENT_LINE_SHADER_SPV): $(NEW_VULKAN_FRAGMENT_LINE_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
$(NEW_VULKAN_FRAGMENT_HUD_SHADER_SPV): $(NEW_VULKAN_FRAGMENT_HUD_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
$(NEW_VULKAN_FRAGMENT_HUD_CURSOR_SHADER_SPV): $(NEW_VULKAN_FRAGMENT_HUD_CURSOR_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
$(NEW_VULKAN_FRAGMENT_HUD_LINE_SHADER_SPV): $(NEW_VULKAN_FRAGMENT_HUD_LINE_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
$(NEW_VULKAN_FRAGMENT_HUD_TEXT_SHADER_SPV): $(NEW_VULKAN_FRAGMENT_HUD_TEXT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
$(NEW_VULKAN_VERTEX_CURSOR_SHADER_SPV): $(NEW_VULKAN_VERTEX_CURSOR_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@
$(NEW_VULKAN_VERTEX_NOTE_SHADER_SPV): $(NEW_VULKAN_VERTEX_NOTE_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@
$(NEW_VULKAN_VERTEX_TEXT_SHADER_SPV): $(NEW_VULKAN_VERTEX_TEXT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@
$(NEW_VULKAN_VERTEX_LINE_SHADER_SPV): $(NEW_VULKAN_VERTEX_LINE_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@
$(NEW_VULKAN_VERTEX_HUD_SHADER_SPV): $(NEW_VULKAN_VERTEX_HUD_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@
$(NEW_VULKAN_VERTEX_HUD_CURSOR_SHADER_SPV): $(NEW_VULKAN_VERTEX_HUD_CURSOR_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@
$(NEW_VULKAN_VERTEX_HUD_LINE_SHADER_SPV): $(NEW_VULKAN_VERTEX_HUD_LINE_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@
$(NEW_VULKAN_VERTEX_HUD_TEXT_SHADER_SPV): $(NEW_VULKAN_VERTEX_HUD_TEXT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@

$(UNITY_O): $(UNITY_C)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $(UNITY_C) -o $@

$(TARGET): $(UNITY_O) $(H_BUILD_FILES) $(SPV_BUILD_FILES)
	$(Q)$(CC) $(CFLAGS) -o $@ $(UNITY_O) $(LDFLAGS)

clean:
	$(Q)rm -rf $(BUILD_DIR) $(TARGET) 

gcc_check:
	$(Q)$(CC) $(CFLAGS) -fsyntax-only $(SRC)

-include $(DEP)

#-----------------------------------------------------------------------------
# war-devel
#-----------------------------------------------------------------------------
WAR_VERSION := 0.1.0-nightly-$(shell date +%Y%m%d%H%M)
PREFIX ?= /usr
INCLUDEDIR := $(PREFIX)/include/war
PCDIR := $(PREFIX)/lib/pkgconfig
COMPILE_COMMANDS_PATH := $(HOME)/.config/war/compile_commands.json

.PHONY: install-devel clean-devel

install-devel: $(H_BUILD_FILES)
	@echo "Installing WAR development files..."
	# Install headers
	@mkdir -p $(INCLUDEDIR)
	@cp -r src/h/* $(INCLUDEDIR)/

	# Generate pkg-config file dynamically
	@mkdir -p $(PCDIR)
	@PIPEWIRE_CFLAGS="$$(pkg-config --cflags libpipewire-0.3)" ; \
	{ \
	  echo "prefix=$(PREFIX)"; \
	  echo "includedir=\$${prefix}/include/war"; \
	  echo "Name: war-devel"; \
	  echo "Description: WAR development files (headers, scripts)"; \
	  echo "Version: $(WAR_VERSION)"; \
	  echo "Cflags: -I\$${includedir} -I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2 -D_REENTRANT"; \
	} > $(PCDIR)/war-devel.pc

	# Generate compile_commands.json in ~/.config/war
	@mkdir -p $(dir $(COMPILE_COMMANDS_PATH)) ; \
	PIPEWIRE_CFLAGS="$$(pkg-config --cflags libpipewire-0.3)" ; \
	CFLAGS="-I/usr/include/war -I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2 -D_REENTRANT $$PIPEWIRE_CFLAGS" ; \
	echo "[" > $(COMPILE_COMMANDS_PATH) ; \
	for srcfile in $(shell find src -name '*.c'); do \
	  echo "  {" >> $(COMPILE_COMMANDS_PATH) ; \
	  echo "    \"directory\": \"$(PWD)\"," >> $(COMPILE_COMMANDS_PATH) ; \
	  echo "    \"command\": \"gcc $$CFLAGS -c $$srcfile\"," >> $(COMPILE_COMMANDS_PATH) ; \
	  echo "    \"file\": \"$$srcfile\"" >> $(COMPILE_COMMANDS_PATH) ; \
	  echo "  }," >> $(COMPILE_COMMANDS_PATH) ; \
	done ; \
	# remove trailing comma and close JSON array
	sed -i '$$ s/,$$//' $(COMPILE_COMMANDS_PATH) ; \
	echo "]" >> $(COMPILE_COMMANDS_PATH) ; \
	echo "compile_commands.json generated at $(COMPILE_COMMANDS_PATH)"

clean-devel:
	@echo "Cleaning WAR development install..."
	@rm -rf $(INCLUDEDIR) $(PCDIR)/war-devel.pc $(COMPILE_COMMANDS_PATH)
