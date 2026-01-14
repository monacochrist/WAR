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
QUAD_VERT_SHADER_SRC := $(SHADER_SRC_DIR)/war_quad_vertex.glsl
QUAD_FRAG_SHADER_SRC := $(SHADER_SRC_DIR)/war_quad_fragment.glsl
QUAD_VERT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_quad_vertex.spv
QUAD_FRAG_SHADER_SPV := $(SHADER_BUILD_DIR)/war_quad_fragment.spv
TEXT_VERT_SHADER_SRC := $(SHADER_SRC_DIR)/war_text_vertex.glsl
TEXT_FRAG_SHADER_SRC := $(SHADER_SRC_DIR)/war_text_fragment.glsl
TEXT_VERT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_text_vertex.spv
TEXT_FRAG_SHADER_SPV := $(SHADER_BUILD_DIR)/war_text_fragment.spv
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
SPV_BUILD_FILES := \
	$(QUAD_VERT_SHADER_SPV) \
	$(QUAD_FRAG_SHADER_SPV) \
	$(TEXT_VERT_SHADER_SPV) \
	$(TEXT_FRAG_SHADER_SPV) \
	$(NSGT_VERT_SHADER_SPV) \
	$(NSGT_FRAG_SHADER_SPV) \
	$(NSGT_COMPUTE_NSGT_SHADER_SPV) \
	$(NSGT_COMPUTE_MAGNITUDE_SHADER_SPV) \
	$(NSGT_COMPUTE_TRANSIENT_SHADER_SPV) \
	$(NSGT_COMPUTE_IMAGE_SHADER_SPV) \
	$(NSGT_COMPUTE_WAV_SHADER_SPV) \

H_BUILD_DIR := $(BUILD_DIR)/h
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

$(QUAD_VERT_SHADER_SPV): $(QUAD_VERT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@
$(QUAD_FRAG_SHADER_SPV): $(QUAD_FRAG_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
$(TEXT_VERT_SHADER_SPV): $(TEXT_VERT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@
$(TEXT_FRAG_SHADER_SPV): $(TEXT_FRAG_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@
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
