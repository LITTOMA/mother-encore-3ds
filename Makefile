# Desktop and 3DS front door. Use devkitPro MSYS2 make on Windows, not nmake.
.DEFAULT_GOAL := help
PYTHON ?= python3
CTEST_ARGS ?=
BUILD_JOBS ?= 2
TEX3DS ?= $(DEVKITPRO)/tools/bin/tex3ds
.PHONY: help content native-content items-assets audio-assets battle-assets round-assets house-assets assets map-assets actor-assets house-layers new-game-assets host test sanitize 3dsx cia cxi 3ds release doctor clean
help:
	@echo "make host/test/sanitize | make 3dsx/cia/cxi/3ds/release"
	@echo "3DS targets require devkitPro 3ds-dev; CIA/CCI also require makerom + bannertool."
content: native-content
	$(PYTHON) tools/content_compiler.py
native-content:
	$(PYTHON) tools/restore_audio.py
	$(PYTHON) tools/blackbars_assets.py compile
	$(PYTHON) tools/native_input.py compile
	$(PYTHON) tools/native_phone.py compile
	$(PYTHON) tools/world_effect_assets.py compile
	$(PYTHON) tools/doll_entry_asset.py compile
	$(PYTHON) tools/pillow_entry_asset.py compile
	$(PYTHON) tools/native_content.py compile
	$(PYTHON) tools/native_battle.py compile
	$(PYTHON) tools/native_round.py compile
	$(PYTHON) tools/doll_round.py compile
	$(PYTHON) tools/pillow_round.py compile
	$(PYTHON) tools/native_house.py compile
	$(PYTHON) tools/items_assets.py verify
	$(PYTHON) tools/native_items.py compile
	$(PYTHON) tools/native_session.py compile
	$(PYTHON) tools/session_migration.py compile
	$(PYTHON) tools/native_restore.py compile
	$(PYTHON) tools/continue_assets.py compile
	$(PYTHON) tools/loading_indicator_assets.py compile
	$(PYTHON) tools/new_game_assets.py compile
	$(PYTHON) tools/startup_settings_assets.py compile
	$(PYTHON) tools/startup_settings_assets.py verify
	$(PYTHON) tools/house_button_prompt_assets.py verify
	$(PYTHON) tools/localization_assets.py compile
	$(PYTHON) tools/native_introduction.py compile
	$(PYTHON) tools/resource_catalog.py compile
assets:
	$(PYTHON) tools/generate_branding.py
audio-assets:
	$(PYTHON) tools/audio_asset.py compile
items-assets:
	$(PYTHON) tools/items_assets.py compile --tex3ds "$(TEX3DS)"
house-assets:
	$(PYTHON) tools/house_assets.py compile --tex3ds "$(TEX3DS)"
round-assets:
	$(PYTHON) tools/round_assets.py compile --tex3ds "$(TEX3DS)"
doll-assets:
	$(PYTHON) tools/doll_entry_asset.py assets --tex3ds "$(TEX3DS)"
battle-assets:
	$(PYTHON) tools/battle_assets.py compile --tex3ds "$(TEX3DS)"
map-assets:
	$(PYTHON) tools/map_asset.py prepare
	$(PYTHON) tools/map_asset.py compile --tex3ds "$(TEX3DS)"
actor-assets:
	$(PYTHON) tools/actor_asset.py compile --tex3ds "$(TEX3DS)"
house-layers:
	$(PYTHON) tools/house_layers.py compile --tex3ds "$(TEX3DS)"
host: content
	cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
	cmake --build build/host --parallel $(BUILD_JOBS)
test: host
	ctest --test-dir build/host --output-on-failure $(CTEST_ARGS)
sanitize: content
	cmake -S . -B build/sanitize -DCMAKE_BUILD_TYPE=Debug -DENCORE_SANITIZERS=ON
	cmake --build build/sanitize --parallel $(BUILD_JOBS)
	ctest --test-dir build/sanitize --output-on-failure $(CTEST_ARGS)
introduction-assets:
	$(PYTHON) tools/introduction_assets.py compile --tex3ds "$(TEX3DS)" --godot "$(GODOT3)"
3dsx: content assets
	$(PYTHON) tools/introduction_assets.py verify
	$(PYTHON) tools/native_introduction.py verify
	$(PYTHON) tools/blackbars_assets.py verify
	$(PYTHON) tools/native_session.py verify
	$(PYTHON) tools/session_migration.py verify
	$(PYTHON) tools/native_restore.py verify
	$(PYTHON) tools/continue_assets.py verify
	$(PYTHON) tools/loading_indicator_assets.py verify
	$(PYTHON) tools/new_game_assets.py verify
	$(PYTHON) tools/startup_settings_assets.py verify
	$(PYTHON) tools/house_button_prompt_assets.py verify
	$(PYTHON) tools/dialogue_choice_assets.py verify
	$(PYTHON) tools/save_menu_assets.py verify
	$(PYTHON) tools/native_input.py verify
	$(PYTHON) tools/native_phone.py verify
	$(PYTHON) tools/world_effect_assets.py verify
	$(PYTHON) tools/native_content.py verify
	$(PYTHON) tools/doll_entry_asset.py verify
	$(PYTHON) tools/pillow_entry_asset.py verify
	$(PYTHON) tools/pillow_round.py verify
	$(PYTHON) tools/native_battle.py verify
	$(PYTHON) tools/native_round.py verify
	$(PYTHON) tools/native_house.py verify
	$(PYTHON) tools/house_assets.py verify
	$(PYTHON) tools/round_assets.py verify
	$(PYTHON) tools/audio_asset.py verify
	$(PYTHON) tools/battle_assets.py verify
	$(PYTHON) tools/verify_lamp_sources.py
	$(PYTHON) tools/map_asset.py verify
	$(PYTHON) tools/actor_asset.py verify
	$(PYTHON) tools/house_layers.py verify
	$(MAKE) -f platform/ctr/Makefile all
cia: 3dsx
	$(PYTHON) tools/package_ctr.py --format cia
cxi: 3dsx
	$(PYTHON) tools/package_ctr.py --format cxi
3ds: 3dsx
	$(PYTHON) tools/package_ctr.py --format cci
release: cia
	$(PYTHON) tools/release.py
doctor:
	$(PYTHON) tools/doctor.py
clean:
	$(PYTHON) -c "import shutil; [shutil.rmtree(p,ignore_errors=True) for p in ('build','dist')]"

loading-indicator-assets:
	$(PYTHON) tools/loading_indicator_assets.py assets --tex3ds "$(TEX3DS)"

new-game-assets:
	$(PYTHON) tools/new_game_assets.py assets --tex3ds "$(TEX3DS)" --godot "$(GODOT3)"
