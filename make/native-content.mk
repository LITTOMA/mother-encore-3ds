# GNU make owns the resource DAG; phony nodes retain every source admission.
# Normal compile/verify commands only: texture extraction is an explicit task.
.DEFAULT_GOAL := native-content
PYTHON ?= python3
# Per-invocation identity distinguishes blocked tasks' older diagnostic receipts.
export ENCORE_CONTENT_RUN_ID := $(shell "$(PYTHON)" -c "import uuid; print(uuid.uuid4().hex)")
$(info CONTENT RUN $(ENCORE_CONTENT_RUN_ID))
CONTENT_RUNNER ?= "$(PYTHON)" tools/run_content_task.py
CONTENT_TASKS := audio bars input phone effects doll-entry pillow-entry room battle \
 round doll-round pillow-round house house-presents items-check items session migration restore \
 continue loading naming settings prompts locale introduction inspections drawer storage item-details field-equipment
.PHONY: native-content $(CONTENT_TASKS) catalog encounters
native-content: catalog encounters
	@echo "CONTENT RUN $(ENCORE_CONTENT_RUN_ID) complete"
# Both final fingerprints must observe a complete generation, never partial writes.
catalog encounters: $(CONTENT_TASKS)
# The room provenance admits the reviewed entry/effect packs. Lamp battle reads
# the room, while room admission checks the preceding reviewed Lamp pack: do
# not rewrite that pack concurrently with admission (the existing contract).
room: audio effects doll-entry pillow-entry
battle: room
restore: room house
items: items-check
# Localization writes its legacy-lane IR; finish frozen migration admission first.
locale: migration house-presents
# Intro admission reads its audio bank and immutable scene/font inputs.
introduction: audio
inspections: house
drawer: house
house-presents: house
storage: items
item-details: items
field-equipment: items item-details
session: drawer items storage house-presents

audio:
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
house-presents:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/house_presents.py compile
inspections:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/house_inspection.py compile
drawer:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/drawer_program.py compile
storage:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_storage.py compile
item-details:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/item_details.py compile
field-equipment:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/field_equipment.py compile
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
