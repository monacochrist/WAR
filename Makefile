CC := gcc
TEST ?= 0

ifeq ($(TEST), 1)
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O0 -g -march=x86-64 -std=c99 -MMD -DDEBUG -I src
else
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O3 -march=x86-64 -std=c99 -MMD -DNDEBUG -I src
endif

LIBSODIUM_DIR := vendor/libsodium-1.0.21

PKG_CONFIG_CFLAGS_PACKAGES := \
freetype2 \
xkbcommon \
libpipewire-0.3 \
luajit
PKG_CONFIG_CFLAGS_PACKAGES_STR := $(subst \, ,$(PKG_CONFIG_CFLAGS_PACKAGES))
PKG_CONFIG_CFLAGS := $(shell pkg-config --cflags $(PKG_CONFIG_CFLAGS_PACKAGES))
EXPLICIT_CFLAGS_PACKAGES :=
EXPLICIT_CFLAGS := -I$(LIBSODIUM_DIR)/include

PKG_CONFIG_LIBS_PACKAGES := \
freetype2 \
xkbcommon \
libpipewire-0.3 \
luajit \
vulkan
PKG_CONFIG_LIBS := $(shell pkg-config --libs $(PKG_CONFIG_LIBS_PACKAGES))
EXPLICIT_LIBS_PACKAGES :=
EXPLICIT_LIBS := -lm -lpthread -lasound -L$(LIBSODIUM_DIR)/.libs -lsodium

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
	$(Q)$(CC) $(CFLAGS) $(PKG_CONFIG_CFLAGS) $(EXPLICIT_CFLAGS) -c $(UNITY_C) -o $@

$(TARGET): $(UNITY_O) $(H_BUILD_FILES) $(SPV_BUILD_FILES)
	$(Q)$(CC) -o $@ $(UNITY_O) $(PKG_CONFIG_LIBS) $(EXPLICIT_LIBS)

clean:
	$(Q)rm -rf $(BUILD_DIR) $(TARGET) 

gcc_check:
	$(Q)$(CC) $(CFLAGS) $(PKG_CONFIG_CFLAGS) $(EXPLICIT_CFLAGS) -fsyntax-only $(SRC)

-include $(DEP)

# key

.PHONY: 

WAR_KEY_DIR := key
WAR_KEY_C := $(WAR_KEY_DIR)/key.c

key_gen:
	$(CC) -I$(LIBSODIUM_DIR)/include $(WAR_KEY_C) -o key/gen_key -L$(LIBSODIUM_DIR)/.libs -lsodium
	./key/gen_key
	rm -f key/gen_key

# war

.PHONY: war_pkgbuild war_devel war_devel_pkgbuild pkgbuild

define WAR_PKGBUILD_TEMPLATE
pkgname=war
pkgver=0
pkgrel=1
pkgdesc="WAR runtime files"
arch=('x86_64')
url="https://github.com/monacochrist/WAR"
license=('custom:WAR')
depends=('pipewire')
makedepends=('git' 'pkgconf' 'make')
source=("WAR::git+https://github.com/monacochrist/WAR.git")
sha256sums=('SKIP')

pkgver() {
    cd "$$srcdir/WAR"
    if [ -d .git ]; then
        printf "r%s.%s" "$$(git rev-list --count HEAD)" "$$(git rev-parse --short=7 HEAD)"
	else
		date +%Y%m%d    
	fi
}

build() {
	cd "$$srcdir/WAR" || exit 1
	make all
}

package() {
	cd "$$srcdir/WAR" || exit 1
	# Install runtime binary
	install -Dm755 WAR "$$pkgdir/usr/bin/war"

	# Install any runtime shared libs (if any)
	# install -Dm755 build/libwar.so "$$pkgdir/usr/lib/libwar.so"

	# Optionally, install docs
	# install -Dm644 README.md "$$pkgdir/usr/share/doc/war/README.md"
}
endef

war_pkgbuild:
	@$(file >war/PKGBUILD,$(WAR_PKGBUILD_TEMPLATE))

# war-devel

DESTDIR ?=

define WAR_DEVEL_PC_TEMPLATE
prefix=/usr
includedir=\$${prefix}/include/war

Name: war-devel
Description: WAR development files
Version: $$pkgver
Requires: $(PKG_CONFIG_CFLAGS_PACKAGES_STR)
Cflags: -I\$${includedir}
endef

war_devel:
	@install -dm755 $(DESTDIR)/usr/include/war
	@install -m644 src/h/* $(DESTDIR)/usr/include/war/
	@install -dm755 $(DESTDIR)/usr/lib/pkgconfig
	@$(file >$(DESTDIR)/usr/lib/pkgconfig/war-devel.pc,$(WAR_DEVEL_PC_TEMPLATE))

define WAR_DEVEL_PKGBUILD_TEMPLATE
pkgname=war-devel
pkgver=0
pkgrel=1
pkgdesc="WAR development files"
arch=('x86_64')
url="https://github.com/monacochrist/WAR"
license=('custom:WAR')
depends=('pipewire')
makedepends=('git' 'pkgconf' 'make')
source=("WAR::git+https://github.com/monacochrist/WAR.git")
sha256sums=('SKIP')

pkgver() {
    cd "$$srcdir/WAR"
    if [ -d .git ]; then
        printf "r%s.%s" "$$(git rev-list --count HEAD)" "$$(git rev-parse --short=7 HEAD)"
	else
		date +%Y%m%d    
	fi
}

package() {
	cd "$$srcdir/WAR" || exit 1
	make DESTDIR="$$pkgdir" war_devel
}
endef

war_devel_pkgbuild:
	@$(file >war-devel/PKGBUILD,$(WAR_DEVEL_PKGBUILD_TEMPLATE))

pkgbuild: war_pkgbuild war_devel_pkgbuild
