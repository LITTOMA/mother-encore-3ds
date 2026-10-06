# Explicit source derivation shares the normal resource graph. No extractor
# runs in normal builds. Each producer retains its own source/pin admission.
export ENCORE_GENERATION_REFRESH := $(REFRESH_SOURCES)
.PHONY: source-fonts
catalog: source-fonts
source-fonts: locale field-psi field-cash-box field-shop field-item-details field-inventory
source-fonts:
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/content_pipeline.py fonts

ifeq ($(REFRESH_SOURCES),1)
SOURCE_ART_TASKS := $(addprefix source-art-,canvas doll pillow item-details field-equipment field-item-details sparkles basement-actors storage drawer-item shop cash-box payphone vending introduction)
.PHONY: $(SOURCE_ART_TASKS)
$(SOURCE_ART_TASKS): source-art-%:
	$(CONTENT_RUNNER) source-art-$* -- "$(PYTHON)" tools/content_pipeline.py asset --producer $*
field-canvas-art: source-art-canvas
doll-entry: source-art-doll
pillow-entry: source-art-pillow
item-details: source-art-item-details
field-equipment: source-art-field-equipment
field-item-details: source-art-field-item-details
sparkles: source-art-sparkles
basement-actors: source-art-basement-actors
storage: source-art-storage
drawer: source-art-drawer-item
field-shop: source-art-shop
field-cash-box: source-art-cash-box
field-payphone: source-art-payphone
field-vending: source-art-vending
introduction: source-art-introduction
source-art-field-equipment: items item-details
source-art-item-details source-art-storage: items
source-art-field-item-details source-art-shop: field-item-definitions
source-art-vending: field-shop
source-art-canvas: field-node-tree
.PHONY: source-linked source-round source-doll-round source-pillow-round source-session source-restore source-naming source-prompts source-introduction
room house audio: source-linked
round: source-round
doll-round: source-doll-round
pillow-round: source-pillow-round
session: source-session
restore: source-restore
naming: source-naming
prompts: source-prompts
introduction: source-introduction
source-introduction: source-art-introduction audio
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_introduction.py extract
source-linked: effects doll-entry pillow-entry basement basement-actors basement-music
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/link_phone_content.py
source-round: source-linked
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/extract_battle_round.py
source-doll-round: source-round
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/doll_round.py extract
source-pillow-round: source-doll-round
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/pillow_round.py extract
source-session: source-linked source-pillow-round drawer items storage item-use basement
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_session.py extract
source-restore: room house source-session
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/native_restore.py extract
source-naming: source-session source-pillow-round
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/new_game_assets.py bindings
source-prompts: house inspections
	$(CONTENT_RUNNER) $@ -- "$(PYTHON)" tools/house_button_prompt_assets.py bindings
endif
