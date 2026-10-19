.DEFAULT_GOAL := all
.SUFFIXES:
ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is unset. Install devkitPro 3ds-dev, then source its environment)
endif
DEVKITARM ?= $(DEVKITPRO)/devkitARM
ifeq ($(wildcard $(DEVKITARM)/3ds_rules),)
$(error Missing $(DEVKITARM)/3ds_rules. Run make doctor)
endif
include $(DEVKITARM)/3ds_rules
ROOT := $(CURDIR)
CTRULIB := $(DEVKITPRO)/libctru
CXX := $(DEVKITARM)/bin/arm-none-eabi-g++
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CPPFLAGS := -D__3DS__ -I$(ROOT)/include -I$(CTRULIB)/include $(EXTRA_CPPFLAGS)
CXXFLAGS := $(ARCH) -std=gnu++17 -O2 -g -Wall -Wextra -Wpedantic -mword-relocations -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti
LDFLAGS := $(ARCH) -specs=3dsx.specs -g -Wl,--gc-sections,-Map,$(ROOT)/dist/encore-native.map
LDLIBS := -L$(CTRULIB)/lib -lcitro2d -lcitro3d -lctru -lm
SOURCES := runtime/storage_data.cpp runtime/storage_menu.cpp runtime/drawer_program.cpp runtime/house_inspection_data.cpp runtime/resource_catalog.cpp runtime/localization.cpp runtime/localized_presentation.cpp runtime/locale_preference.cpp runtime/source_font.cpp runtime/title_locale_data.cpp runtime/introduction.cpp runtime/new_game_setup.cpp runtime/startup_settings.cpp runtime/house_button_prompts.cpp runtime/loading_indicator_data.cpp runtime/continue_menu_data.cpp runtime/continue_menu.cpp runtime/slot_preference.cpp runtime/restore_data.cpp runtime/session_restore.cpp runtime/session_migration.cpp runtime/load_rng.cpp runtime/fresh_house.cpp runtime/dialogue_choices_data.cpp runtime/dialogue_choices.cpp runtime/phone_data.cpp runtime/phone_runtime.cpp runtime/native_session.cpp runtime/session_save.cpp runtime/save_menu_data.cpp runtime/save_menu.cpp runtime/native_input.cpp runtime/world_effect_data.cpp runtime/world_effect.cpp runtime/items_data.cpp runtime/items_menu.cpp runtime/house_audio.cpp runtime/house_runtime.cpp runtime/house_data.cpp runtime/house_presentation.cpp runtime/battle_outcome.cpp runtime/battle_round.cpp runtime/battle_round_data.cpp runtime/battle_action_presentation.cpp runtime/source_random.cpp runtime/audio_data.cpp runtime/battle_data.cpp runtime/battle_entry.cpp platform/ctr/audio_player.cpp runtime/music_regions.cpp platform/ctr/music_region_player.cpp platform/ctr/music_region_service.cpp runtime/room_data.cpp runtime/file_io.cpp runtime/progression.cpp runtime/movement.cpp runtime/animation.cpp runtime/collision.cpp runtime/world_flags.cpp runtime/world.cpp runtime/opening_trigger.cpp runtime/dialogue.cpp runtime/actor_actions.cpp runtime/cutscene_camera.cpp platform/ctr/main.cpp
SOURCES += runtime/music_zones.cpp runtime/gameplay_audio.cpp
SOURCES += runtime/field_swing.cpp runtime/field_swing_graphics.cpp runtime/field_swing_action.cpp runtime/field_swing_entry.cpp runtime/field_swing_resources.cpp runtime/prop_clips.cpp runtime/field_props.cpp runtime/field_steps.cpp runtime/field_menus_data.cpp runtime/field_menus.cpp
SOURCES += runtime/item_details.cpp runtime/field_equipment_data.cpp runtime/field_equipment_menu.cpp
SOURCES += runtime/field_data.cpp runtime/world_links.cpp runtime/field_scene.cpp runtime/field_scene_resources.cpp runtime/field_story.cpp runtime/battle_loot.cpp
SOURCES += runtime/present_data.cpp runtime/house_presents.cpp runtime/mick_treats.cpp
SOURCES += runtime/music_player_choice.cpp runtime/music_players.cpp runtime/music_scene_graph.cpp runtime/music_programs.cpp runtime/music_library_bindings.cpp runtime/music_bank_routes.cpp platform/ctr/music_bank_transports.cpp
OBJECTS := $(patsubst %.cpp,build/ctr/%.o,$(SOURCES))
TOOLS := $(DEVKITPRO)/tools/bin
APP := dist/encore-native
# Opt-in only. Normal builds preserve the existing renderer.
EXPERIMENTAL_GPU_BACKGROUND ?= 0
EXPERIMENTAL_GPU_CERTIFICATES ?= 0
EXPERIMENTAL_GPU_TEXTURE_STRIPS ?= 0
# Destructive reference-view shortcut: opt in for developer builds only.
DEBUG_REFERENCE_VIEW ?= 0
ifeq ($(DEBUG_REFERENCE_VIEW),1)
CPPFLAGS += -DENCORE_DEBUG_REFERENCE_VIEW=1
else ifneq ($(DEBUG_REFERENCE_VIEW),0)
$(error DEBUG_REFERENCE_VIEW must be 0 or 1)
endif
.PHONY: reference-view-mode-force
reference-view-mode-force:
build/ctr/reference-view-mode.stamp: reference-view-mode-force
	@mkdir -p $(dir $@)
	@printf '%s\n' '$(DEBUG_REFERENCE_VIEW)' | cmp -s - $@ || printf '%s\n' '$(DEBUG_REFERENCE_VIEW)' > $@
build/ctr/platform/ctr/main.o: build/ctr/reference-view-mode.stamp
.PHONY: gpu-background-mode-force
gpu-background-mode-force:
build/ctr/gpu-background-mode.stamp: gpu-background-mode-force
	@mkdir -p $(dir $@)
	@printf '%s\n' '$(EXPERIMENTAL_GPU_BACKGROUND):$(EXPERIMENTAL_GPU_CERTIFICATES):$(EXPERIMENTAL_GPU_TEXTURE_STRIPS)' | cmp -s - $@ || printf '%s\n' '$(EXPERIMENTAL_GPU_BACKGROUND):$(EXPERIMENTAL_GPU_CERTIFICATES):$(EXPERIMENTAL_GPU_TEXTURE_STRIPS)' > $@
build/ctr/platform/ctr/main.o: build/ctr/gpu-background-mode.stamp
ifeq ($(EXPERIMENTAL_GPU_TEXTURE_STRIPS),1)
CPPFLAGS += -DENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
build/ctr/platform/ctr/main.o: build/ctr/gpu_row_texture_shader.hpp build/ctr/gpu_mapped_texture_shader.hpp
build/ctr/gpu_mapped_texture.shbin: platform/ctr/shaders/gpu_mapped_texture.v.pica platform/ctr/shaders/gpu_mapped_texture.g.pica
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/picasso -o $@ $^
build/ctr/gpu_mapped_texture_shader.hpp: build/ctr/gpu_mapped_texture.shbin tools/embed_gpu_mapped_texture.py
	python3 tools/embed_gpu_mapped_texture.py $< $@
build/ctr/gpu_row_texture.shbin: platform/ctr/shaders/gpu_row_texture.v.pica platform/ctr/shaders/gpu_row_texture.g.pica
	@mkdir -p $(dir $@)
	$(DEVKITPRO)/tools/bin/picasso -o $@ $^
build/ctr/gpu_row_texture_shader.hpp: build/ctr/gpu_row_texture.shbin tools/embed_gpu_row_texture.py
	python3 tools/embed_gpu_row_texture.py $< $@
endif

ifeq ($(EXPERIMENTAL_GPU_BACKGROUND),1)
CPPFLAGS += -DENCORE_EXPERIMENTAL_GPU_BACKGROUND -I$(ROOT)/build/ctr
ifeq ($(EXPERIMENTAL_GPU_CERTIFICATES),1)
CPPFLAGS += -DENCORE_GPU_CERTIFICATE_TABLES
endif
build/ctr/platform/ctr/main.o: build/ctr/gpu_region_shader.hpp
build/ctr/gpu_region.shbin: platform/ctr/shaders/gpu_direct_region.v.pica platform/ctr/shaders/gpu_direct_region.g.pica
	@mkdir -p $(dir $@)
	$(TOOLS)/picasso -o $@ $^
build/ctr/gpu_region_shader.hpp: build/ctr/gpu_region.shbin tools/embed_gpu_program.py
	python3 tools/embed_gpu_program.py $< $@
endif
.PHONY: all stage-romfs
all: $(APP).3dsx
stage-romfs:
	python3 tools/stage_native_romfs.py
build/ctr/platform/ctr/main.o: CXXFLAGS += -ffp-contract=off
build/ctr/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
$(APP).elf: $(OBJECTS)
	@mkdir -p dist
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
$(APP).smdh: assets/icon.png
	@mkdir -p dist
	$(TOOLS)/smdhtool --create "Encore Native Preview" "Mother Encore port development" "Encore Native contributors" $< $@
$(APP).3dsx: $(APP).elf $(APP).smdh stage-romfs $(wildcard romfs/graphics/world/house-map/*) $(wildcard romfs/graphics/actors/*) $(wildcard romfs/graphics/world/house-layers/*) $(wildcard romfs/world/house/* romfs/world/podunk/* romfs/battle/*/* romfs/story/* romfs/session/* romfs/system/* romfs/localization/* romfs/ui/*)
	$(TOOLS)/3dsxtool $(APP).elf $@ --smdh=$(APP).smdh --romfs=$(ROOT)/build/ctr/native-romfs
-include $(OBJECTS:.o=.d)
