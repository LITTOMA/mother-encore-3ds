# GNU make owns the resource DAG; phony nodes retain every source admission.
# Normal compile/verify commands only: texture extraction is an explicit task.
.DEFAULT_GOAL := native-content
PYTHON ?= python3
REFRESH_SOURCES ?= 0
# Per-invocation identity distinguishes blocked tasks' older diagnostic receipts.
export ENCORE_CONTENT_RUN_ID := $(shell "$(PYTHON)" -c "import uuid; print(uuid.uuid4().hex)")
$(info CONTENT RUN $(ENCORE_CONTENT_RUN_ID))
CONTENT_RUNNER ?= "$(PYTHON)" tools/run_content_task.py
CONTENT_TASKS := audio bars input phone effects doll-entry pillow-entry room battle \
 round doll-round pillow-round house items-check items session migration restore \
 continue loading naming settings prompts locale introduction inspections drawer storage item-details field-equipment item-use basement basement-actors basement-music sparkles field-psi field-interact field-lifecycle field-present field-dropped field-sparkles field-openable field-payphone field-cash-box field-butterfly field-cutscene field-birds field-camera-area field-music-changer field-item-definitions field-camera-arrows field-scene-actions field-stepping-sounds field-player-transitions field-game-camera field-door-npc field-melody-background field-programmes field-node-tree field-node-recipe field-dialogue-life field-canvas-art field-native-timer field-global-registry field-dialogue-ui field-shop field-vending field-item-details field-inventory
.PHONY: native-content $(CONTENT_TASKS) catalog encounters
CONTENT_TASKS += field-goods field-global-flags field-dialogue-visual field-dialogue-audio field-ui-manager field-battle-bg-resources field-ui-preloads
.PHONY: field-goods field-global-flags field-dialogue-visual field-dialogue-audio field-ui-manager field-battle-bg-resources field-ui-preloads
native-content: catalog encounters
	@echo "CONTENT RUN $(ENCORE_CONTENT_RUN_ID) complete"
# Both final fingerprints must observe a complete generation, never partial writes.
catalog encounters: $(CONTENT_TASKS)
# The room provenance admits the reviewed entry/effect packs. Lamp battle reads
# the room, while room admission checks the preceding reviewed Lamp pack: do
# not rewrite that pack concurrently with admission (the existing contract).
room: audio effects doll-entry pillow-entry basement basement-actors basement-music
battle: room
restore: room house
items: items-check
# Localization writes its legacy-lane IR; finish frozen migration admission first.
migration: session
locale: migration
# Intro admission reads its audio bank and immutable scene/font inputs.
introduction: audio
inspections: house
drawer: house
storage: items
item-details: items
field-equipment: items item-details
field-lifecycle: field-vending field-node-tree field-melody-background field-game-camera field-door-npc field-camera-arrows field-scene-actions field-stepping-sounds field-player-transitions field-cutscene field-birds field-camera-area field-music-changer field-interact field-present field-dropped field-sparkles field-openable field-payphone field-butterfly
field-interact:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_interact_dialog.py compile
field-lifecycle:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_scene_host.py compile
field-psi: items
item-use: items
session: drawer items storage item-use basement

basement:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/basement_progression.py compile
sparkles:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/present_sparkles.py verify
basement-actors:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/basement_actor_assets.py verify
basement-music:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/basement_music_regions.py compile

audio: item-use basement field-psi
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/restore_audio.py
bars:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/blackbars_assets.py compile
input:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_input.py compile
phone:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_phone.py compile
effects:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/world_effect_assets.py compile
doll-entry:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/doll_entry_asset.py compile
pillow-entry:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/pillow_entry_asset.py compile
room:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_content.py compile
battle:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_battle.py compile
round:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_round.py compile
doll-round:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/doll_round.py compile
pillow-round:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/pillow_round.py compile
house:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_house.py compile
inspections:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/house_inspection.py compile
drawer:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/drawer_program.py compile
storage:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_storage.py compile
item-details:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/item_details.py compile
field-psi:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_psi.py compile
field-equipment:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_equipment.py compile
item-use:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/item_use.py compile
items-check:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/items_assets.py verify
items:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_items.py compile
session:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_session.py compile
migration:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/session_migration.py compile
restore:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_restore.py compile
continue:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/continue_assets.py compile
loading:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/loading_indicator_assets.py compile
naming:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/new_game_assets.py compile
settings:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/startup_settings_assets.py compile
	$(CONTENT_RUNNER) settings-check -- "$(PYTHON)" tools/startup_settings_assets.py verify
prompts:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/house_button_prompt_assets.py verify
locale:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/localization_assets.py compile
catalog:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/resource_catalog.py compile
encounters:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/encounter_dependencies.py compile
introduction:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_introduction.py compile

field-present:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_present.py compile

field-dropped:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_dropped.py compile

field-sparkles:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_sparkles.py compile

field-openable:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_openable_door.py compile

field-payphone:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_payphone.py pack

field-cash-box:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_cash_box.py pack

field-butterfly:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_butterfly.py compile

field-cutscene:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_cutscene_area.py compile

field-birds:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_birds.py compile

field-camera-area:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_camera_area.py compile

field-music-changer:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_music_changer.py compile

field-item-definitions:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_item_definitions.py compile

field-camera-arrows:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_camera_arrows.py compile

field-scene-actions:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_scene_actions.py compile

field-stepping-sounds:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_stepping_sounds.py compile

field-player-transitions:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_player_transitions.py compile

field-game-camera:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_game_camera.py compile

field-door-npc:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_door_npc.py compile

field-melody-background:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_melody_background.py compile

field-programmes:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_programme.py compile

field-node-tree:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_node_tree.py compile

field-shop: field-item-definitions
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_shop.py pack

field-vending: field-shop
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_vending_machine.py pack

field-item-details: field-item-definitions
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_item_details.py compile

field-node-recipe:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_node_recipe.py compile

field-dialogue-life: field-node-recipe field-programmes
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_dialogue_lifecycle.py compile

field-canvas-art: field-node-tree
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_canvas_art.py compile

field-native-timer: field-node-tree field-node-recipe
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_native_timer.py compile

field-global-registry:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_global_registry.py compile

field-dialogue-ui: field-node-recipe house
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_dialogue_ui.py compile

field-inventory: field-item-definitions field-item-details field-payphone field-shop
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_inventory.py compile

field-goods: field-inventory
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_goods.py compile
field-global-flags:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_global_flags.py compile
field-dialogue-visual: field-node-recipe field-camera-arrows field-game-camera
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_dialogue_visual.py compile
field-dialogue-audio: field-node-recipe audio
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_dialogue_audio.py compile
field-ui-manager:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_ui_manager.py compile
field-battle-bg-resources:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_battle_bg_resources.py compile
field-ui-preloads: field-ui-manager field-node-recipe
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_ui_preloads.py compile

# The audio bank links Goods source sounds in both check and refresh modes.
audio: field-goods

include make/refresh-content.mk
