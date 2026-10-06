cmake_minimum_required(VERSION 3.16)
project(encore_native VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Python3 3.9 COMPONENTS Interpreter REQUIRED)
option(ENCORE_SANITIZERS "Enable ASan and UBSan on the desktop host" OFF)
option(ENCORE_REGENERATE_NATIVE_CONTENT "Regenerate native resources during CMake builds" ON)
option(ENCORE_TEST_PARALLEL "Enable the reviewed parallel CI test schedule" ON)
add_library(encore_core
    runtime/house_inspection_data.cpp
    runtime/field_equipment_data.cpp runtime/field_equipment_menu.cpp runtime/item_use.cpp runtime/storage_data.cpp runtime/storage_menu.cpp runtime/item_details.cpp runtime/drawer_program.cpp
    runtime/resource_catalog.cpp runtime/localization.cpp runtime/localized_presentation.cpp runtime/locale_preference.cpp runtime/source_font.cpp runtime/title_locale_data.cpp
    runtime/basement_progression.cpp runtime/basement_actor_assets.cpp runtime/present_sparkles.cpp
    runtime/field_scene_data.cpp runtime/field_scene_host.cpp runtime/field_dandelion.cpp
    runtime/field_prompts_data.cpp runtime/field_prompts.cpp
    runtime/field_psi.cpp
    runtime/field_psi_data.cpp
    runtime/field_psi_telepathy.cpp
    runtime/field_present.cpp
    runtime/field_present_data.cpp
    runtime/field_dropped.cpp
    runtime/field_dropped_data.cpp
    runtime/field_sparkles.cpp
    runtime/field_sparkles_data.cpp
    runtime/field_openable_door.cpp
    runtime/field_openable_door_data.cpp
    runtime/field_payphone.cpp
    runtime/field_payphone_data.cpp
    runtime/field_cash_box.cpp
    runtime/field_cash_box_data.cpp
    runtime/field_butterfly.cpp
    runtime/field_butterfly_data.cpp
    runtime/field_sparkles_owner.cpp
    runtime/field_game_camera.cpp
    runtime/field_game_camera_data.cpp
    runtime/field_door_npc.cpp
    runtime/field_door_npc_data.cpp
    runtime/field_melody_background.cpp
    runtime/field_melody_background_data.cpp
    runtime/field_programme.cpp
    runtime/field_programme_data.cpp
    runtime/field_node_tree.cpp
    runtime/field_node_tree_data.cpp
    runtime/field_node_recipe_data.cpp
    runtime/field_dialogue_lifecycle.cpp
    runtime/field_dialogue_lifecycle_data.cpp
    runtime/field_canvas_art.cpp
    runtime/field_canvas_art_data.cpp
    runtime/field_native_timer.cpp
    runtime/field_native_timer_data.cpp
    runtime/field_global_registry.cpp
    runtime/field_global_registry_data.cpp
    runtime/field_native_root_data.cpp
    runtime/field_global_data.cpp runtime/field_global_data_data.cpp
    runtime/global_item_cache.cpp
    runtime/global_yaml_caches.cpp runtime/global_yaml_caches_data.cpp
    runtime/global_packed_directory.cpp runtime/global_packed_directory_data.cpp
    runtime/global_yaml_file.cpp runtime/global_yaml_file_data.cpp
    runtime/global_data_constructor.cpp runtime/global_data_constructor_data.cpp
    runtime/field_character_load.cpp runtime/field_character_load_data.cpp
    runtime/global_load_runtime.cpp runtime/global_load_data.cpp
    runtime/field_global_constructor.cpp runtime/field_global_constructor_data.cpp
    runtime/player_initialization.cpp runtime/player_initialization_data.cpp
    runtime/global_child_ready.cpp runtime/global_child_ready_data.cpp
    runtime/player_ready.cpp runtime/player_ready_data.cpp runtime/player_ready_animation.cpp
    runtime/player_effects.cpp runtime/player_effects_data.cpp
    runtime/player_motion.cpp runtime/player_motion_data.cpp
    runtime/podunk_player_kinematic.cpp runtime/podunk_player_kinematic_solver.cpp
    runtime/podunk_player_character.cpp
    runtime/global_ready_data.cpp
    runtime/player_fetcher.cpp runtime/player_fetcher_data.cpp
    runtime/player_resources_data.cpp
    runtime/house_global_bridge.cpp runtime/house_global_bridge_data.cpp
    runtime/player_graphics_data.cpp
    runtime/player_visual_scripts.cpp runtime/player_visual_scripts_data.cpp
    runtime/field_dialogue_ui.cpp
    runtime/field_dialogue_ui_data.cpp
    runtime/field_inventory.cpp
    runtime/field_inventory_data.cpp
    runtime/field_goods.cpp runtime/field_goods_data.cpp
    runtime/field_global_flags.cpp runtime/field_global_flags_data.cpp
    runtime/field_dialogue_visual.cpp runtime/field_dialogue_visual_data.cpp
    runtime/field_dialogue_audio.cpp runtime/field_dialogue_audio_data.cpp
    runtime/field_ui_manager.cpp runtime/field_ui_manager_data.cpp
    runtime/field_battle_bg_resources.cpp runtime/field_battle_bg_data.cpp
    runtime/field_ui_preloads.cpp runtime/field_ui_preloads_data.cpp
    runtime/field_shop.cpp
    runtime/field_shop_data.cpp
    runtime/field_vending_machine.cpp
    runtime/field_vending_machine_data.cpp
    runtime/field_camera_arrows.cpp
    runtime/field_camera_arrows_data.cpp
    runtime/field_scene_actions.cpp
    runtime/field_scene_actions_data.cpp
    runtime/field_stepping_sounds.cpp
    runtime/field_stepping_sounds_data.cpp
    runtime/field_player_transitions.cpp
    runtime/field_player_transitions_data.cpp
    runtime/field_cutscene_area.cpp
    runtime/field_cutscene_area_data.cpp
    runtime/field_birds.cpp
    runtime/field_birds_data.cpp
    runtime/field_camera_area.cpp
    runtime/field_camera_area_data.cpp
    runtime/field_music_changer.cpp
    runtime/field_music_changer_data.cpp
    runtime/field_item_definitions.cpp
    runtime/field_item_admission.cpp
    runtime/room_music_admission.cpp
    runtime/startup_resource_admission.cpp runtime/catalog_resource_admission.cpp
    runtime/field_item_definitions_data.cpp
    runtime/field_interact_dialog.cpp
    runtime/field_interact_dialog_data.cpp
    runtime/field_dead_bush.cpp
    runtime/field_dead_bush_data.cpp
    runtime/field_door.cpp
    runtime/field_emotes_data.cpp runtime/field_emotes.cpp
    runtime/field_sprite_bridge_data.cpp runtime/field_sprite_bridge.cpp runtime/field_sprite_npc_binding.cpp
    runtime/field_data.cpp runtime/field_runtime.cpp runtime/field_map_space.cpp
    runtime/field_map.cpp runtime/field_motion.cpp runtime/field_geometry.cpp runtime/field_geometry_space.cpp runtime/field_npc_data.cpp runtime/field_npc.cpp runtime/field_enemy_data.cpp runtime/field_enemy.cpp runtime/field_tint.cpp
    runtime/music_regions.cpp runtime/introduction.cpp runtime/new_game_setup.cpp runtime/startup_settings.cpp runtime/house_button_prompts.cpp runtime/loading_indicator_data.cpp runtime/continue_menu_data.cpp runtime/continue_menu.cpp runtime/slot_preference.cpp runtime/restore_data.cpp runtime/session_restore.cpp runtime/session_migration.cpp runtime/load_rng.cpp runtime/fresh_house.cpp runtime/dialogue_choices_data.cpp runtime/dialogue_choices.cpp runtime/phone_data.cpp runtime/phone_runtime.cpp runtime/native_session.cpp runtime/session_save.cpp runtime/save_menu_data.cpp runtime/save_menu.cpp runtime/native_input.cpp runtime/world_effect_data.cpp runtime/world_effect.cpp runtime/items_data.cpp runtime/items_menu.cpp runtime/house_runtime.cpp runtime/house_data.cpp runtime/house_presentation.cpp runtime/battle_outcome.cpp runtime/battle_round.cpp runtime/battle_round_data.cpp runtime/battle_action_presentation.cpp runtime/source_random.cpp runtime/audio_data.cpp runtime/battle_data.cpp runtime/battle_entry.cpp runtime/room_data.cpp runtime/content.cpp runtime/file_io.cpp runtime/story.cpp
    runtime/game.cpp runtime/save.cpp runtime/progression.cpp runtime/movement.cpp runtime/animation.cpp runtime/collision.cpp runtime/world_flags.cpp runtime/world.cpp runtime/opening_trigger.cpp runtime/dialogue.cpp runtime/actor_actions.cpp runtime/cutscene_camera.cpp)
target_include_directories(encore_core PUBLIC include)
if(MSVC)
    target_compile_options(encore_core PRIVATE /W4)
else()
    target_compile_options(encore_core PRIVATE -Wall -Wextra -Wpedantic)
    # Keep the core and all consumers on the same no-RTTI ABI, including UBSan.
    target_compile_options(encore_core PUBLIC -fno-exceptions -fno-rtti)
    if(ENCORE_SANITIZERS)
        target_compile_options(encore_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(encore_core PUBLIC -fsanitize=address,undefined)
        # GCC 12/13 libasan can fail during startup with high-entropy Linux ASLR
        # (GCC PR113430). Keep sanitizer executables at a fixed base without
        # changing the host's ASLR settings or disabling any sanitizer checks.
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
           AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
            target_compile_options(encore_core PUBLIC -fno-pie)
            target_link_options(encore_core PUBLIC -no-pie)
        endif()
    endif()
endif()
add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/build/fixtures/sandbox.encpak
    COMMAND ${Python3_EXECUTABLE} tools/content_compiler.py
    DEPENDS content/sandbox.json tools/content_compiler.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
add_custom_target(encore_content ALL DEPENDS ${CMAKE_SOURCE_DIR}/build/fixtures/sandbox.encpak)
if(ENCORE_REGENERATE_NATIVE_CONTENT)
set(ENCORE_CONTENT_JOBS 4 CACHE STRING "Native resource subprocess limit")
if(NOT ENCORE_CONTENT_JOBS MATCHES "^[0-9]+$" OR ENCORE_CONTENT_JOBS LESS 4)
    message(FATAL_ERROR "ENCORE_CONTENT_JOBS must be at least 4")
endif()
find_program(ENCORE_GNU_MAKE NAMES gmake make)
if(NOT ENCORE_GNU_MAKE)
    message(FATAL_ERROR "Native regeneration requires GNU make (devkitPro MSYS2 on Windows)")
endif()
add_custom_target(encore_native_content ALL
    COMMAND ${ENCORE_GNU_MAKE} --no-print-directory -f make/native-content.mk
        -j${ENCORE_CONTENT_JOBS} "PYTHON=${Python3_EXECUTABLE}" native-content
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
endif()
add_executable(encore_host platform/desktop/main.cpp)
target_link_libraries(encore_host PRIVATE encore_core)
add_dependencies(encore_host encore_content)
include(CTest)
add_compile_definitions(ENCORE_TEST_ROOM_PATH="${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom")
if(BUILD_TESTING)
    add_executable(encore_core_tests tests/core_tests.cpp)
    target_link_libraries(encore_core_tests PRIVATE encore_core)
    add_dependencies(encore_core_tests encore_content)
    add_test(NAME core COMMAND encore_core_tests ${CMAKE_SOURCE_DIR}/build/fixtures/sandbox.encpak)
    add_test(NAME host_smoke COMMAND encore_host --pack ${CMAKE_SOURCE_DIR}/build/fixtures/sandbox.encpak --smoke)
    add_test(NAME python_tools COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/python_suite.py --ctest-dir ${CMAKE_BINARY_DIR})
    set_tests_properties(python_tools PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(core host_smoke python_tools PROPERTIES TIMEOUT 60)
    # The native asset/compiler and corruption suite now includes SDK doubles
    # and source-image verification, plus real temporary Git repositories.
    # Full discovery duplicated the separately registered binding suites and
    # reached 1800.02s on hosted Clang. python_suite.py partitions by exact
    # method IDs, retaining new unregistered modules and every subTest.
    set_tests_properties(python_tools PROPERTIES TIMEOUT 1800)
    add_executable(encore_progression_tests tests/progression_tests.cpp)
    target_link_libraries(encore_progression_tests PRIVATE encore_core)
    add_test(NAME upstream_progression COMMAND encore_progression_tests)
    set_tests_properties(upstream_progression PROPERTIES TIMEOUT 60)
    add_executable(encore_movement_tests tests/movement_tests.cpp)
    target_link_libraries(encore_movement_tests PRIVATE encore_core)
    add_test(NAME upstream_movement COMMAND encore_movement_tests)
    set_tests_properties(upstream_movement PROPERTIES TIMEOUT 60)
    add_executable(encore_animation_tests tests/animation_tests.cpp)
    target_link_libraries(encore_animation_tests PRIVATE encore_core)
    add_test(NAME upstream_animation COMMAND encore_animation_tests)
    set_tests_properties(upstream_animation PROPERTIES TIMEOUT 60)
endif()

if(BUILD_TESTING)
    add_executable(encore_world_tests tests/world_tests.cpp)
    target_link_libraries(encore_world_tests PRIVATE encore_core)
    add_test(NAME opening_world COMMAND encore_world_tests)
    set_tests_properties(opening_world PROPERTIES TIMEOUT 60)
endif()

if(BUILD_TESTING)
    add_executable(encore_collision_tests tests/collision_tests.cpp)
    target_link_libraries(encore_collision_tests PRIVATE encore_core)
    add_test(NAME upstream_collision COMMAND encore_collision_tests)
    set_tests_properties(upstream_collision PROPERTIES TIMEOUT 60)
endif()

if(BUILD_TESTING)
    add_executable(encore_world_flags_tests tests/tests_world_flags.cpp)
    target_link_libraries(encore_world_flags_tests PRIVATE encore_core)
    add_test(NAME upstream_world_flags COMMAND encore_world_flags_tests)
    set_tests_properties(upstream_world_flags PROPERTIES TIMEOUT 60)
endif()

if(BUILD_TESTING)
    foreach(case opening_trigger actor_actions doll_actor_actions doll_postwin_actions pillow_actor_actions dialogue cutscene_camera)
        add_executable(encore_${case}_tests tests/${case}_tests.cpp)
        target_link_libraries(encore_${case}_tests PRIVATE encore_core)
        add_test(NAME upstream_${case} COMMAND encore_${case}_tests)
        set_tests_properties(upstream_${case} PROPERTIES TIMEOUT 60)
    endforeach()
endif()

if(BUILD_TESTING)
 add_executable(encore_room_data_tests tests/room_data_tests.cpp)
 target_link_libraries(encore_room_data_tests PRIVATE encore_core)
 add_test(NAME room_data COMMAND encore_room_data_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom)
endif()
add_executable(encore_data_probe tests/data_only_probe.cpp)
target_link_libraries(encore_data_probe PRIVATE encore_core)
if(BUILD_TESTING)
 add_test(NAME data_only_runtime COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_data_only.py --probe $<TARGET_FILE:encore_data_probe> --dir ${CMAKE_SOURCE_DIR}/reports/data-only-proof-victory)
endif()

if(BUILD_TESTING)
 add_executable(encore_audio_data_tests tests/audio_data_tests.cpp)
 target_link_libraries(encore_audio_data_tests PRIVATE encore_core)
 add_test(NAME audio_data COMMAND encore_audio_data_tests ${CMAKE_SOURCE_DIR}/romfs/sound/banks/opening.encaudio ${CMAKE_SOURCE_DIR}/romfs/)
 add_executable(encore_audio_player_tests tests/audio_player_tests.cpp platform/ctr/audio_player.cpp)
 target_include_directories(encore_audio_player_tests BEFORE PRIVATE tests/fixtures/audio_ndsp platform/ctr)
 target_link_libraries(encore_audio_player_tests PRIVATE encore_core)
 add_test(NAME audio_player_double COMMAND encore_audio_player_tests ${CMAKE_SOURCE_DIR}/romfs/sound/banks/opening.encaudio ${CMAKE_SOURCE_DIR}/romfs/ ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom)
 add_executable(encore_battle_data_tests tests/battle_data_tests.cpp)
 target_link_libraries(encore_battle_data_tests PRIVATE encore_core)
 add_test(NAME battle_data COMMAND encore_battle_data_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle)
endif()

if(BUILD_TESTING)
 add_executable(encore_battle_entry_tests tests/battle_entry_tests.cpp)
 target_link_libraries(encore_battle_entry_tests PRIVATE encore_core)
 add_test(NAME battle_entry COMMAND encore_battle_entry_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom ${CMAKE_SOURCE_DIR}/reports/battle-entry-reference/final/reference.json)
endif()

# Pixel equality uses the frozen pre-optimization renderer as a test-only oracle.
if(BUILD_TESTING AND NOT MSVC)
 set(BACKGROUND_SANITIZER_ARG)
 if(ENCORE_SANITIZERS)
  set(BACKGROUND_SANITIZER_ARG --sanitize)
 endif()
 add_test(NAME background_kernel COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/benchmark_background.py
  --compiler ${CMAKE_CXX_COMPILER} --samples 16 --iterations 2 ${BACKGROUND_SANITIZER_ARG}
  --report ${CMAKE_BINARY_DIR}/background-kernel-check.json)
endif()

if(BUILD_TESTING)
 add_executable(encore_source_random_tests tests/source_random_tests.cpp)
 target_link_libraries(encore_source_random_tests PRIVATE encore_core)
 add_test(NAME source_random COMMAND encore_source_random_tests ${CMAKE_SOURCE_DIR}/reports/battle-round-random/reference.bin)
endif()

if(BUILD_TESTING)
 add_executable(encore_battle_round_data_tests tests/battle_round_data_tests.cpp)
 target_link_libraries(encore_battle_round_data_tests PRIVATE encore_core)
 add_test(NAME battle_round_data COMMAND encore_battle_round_data_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encround)
 add_executable(encore_battle_round_tests tests/battle_round_tests.cpp)
 target_link_libraries(encore_battle_round_tests PRIVATE encore_core)
 add_test(NAME battle_round COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_battle_round_reference.py --probe $<TARGET_FILE:encore_battle_round_tests> --report ${CMAKE_BINARY_DIR}/battle-round-values.json)
endif()

if(BUILD_TESTING)
 add_executable(encore_battle_action_presentation_tests tests/battle_action_presentation_tests.cpp)
 target_link_libraries(encore_battle_action_presentation_tests PRIVATE encore_core)
 add_test(NAME battle_action_presentation COMMAND encore_battle_action_presentation_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encround ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/reports/battle-round-presentation/native-hp-text.tsv)
endif()

if(BUILD_TESTING)
 add_executable(encore_battle_round_integrated_tests tests/battle_round_tests.cpp)
 target_compile_definitions(encore_battle_round_integrated_tests PRIVATE ENCORE_REAL_PRESENTATION)
 target_link_libraries(encore_battle_round_integrated_tests PRIVATE encore_core)
 add_test(NAME battle_round_integrated COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_battle_round_reference.py --probe $<TARGET_FILE:encore_battle_round_integrated_tests> --report ${CMAKE_BINARY_DIR}/battle-round-integrated-values.json)
endif()

if(BUILD_TESTING)
 add_executable(encore_battle_outcome_tests tests/battle_outcome_tests.cpp)
 target_link_libraries(encore_battle_outcome_tests PRIVATE encore_core)
 add_test(NAME battle_outcome COMMAND encore_battle_outcome_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encround ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle)
 set_tests_properties(battle_outcome PROPERTIES TIMEOUT 60)
endif()

if(BUILD_TESTING)
 add_executable(encore_house_data_tests tests/house_data_tests.cpp)
 target_link_libraries(encore_house_data_tests PRIVATE encore_core)
 add_test(NAME house_data COMMAND encore_house_data_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.enchouse)
 add_executable(encore_house_runtime_tests tests/battle_outcome_tests.cpp)
 target_compile_definitions(encore_house_runtime_tests PRIVATE ENCORE_HOUSE_ROUTE_TESTS=1)
 target_link_libraries(encore_house_runtime_tests PRIVATE encore_core)
 add_test(NAME house_runtime COMMAND encore_house_runtime_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encround ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.enchouse)
 set_tests_properties(house_data house_runtime PROPERTIES TIMEOUT 60)
endif()

if(BUILD_TESTING)
 add_executable(encore_house_presentation_tests tests/house_presentation_tests.cpp)
 target_link_libraries(encore_house_presentation_tests PRIVATE encore_core)
 add_test(NAME house_presentation COMMAND encore_house_presentation_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.enchouse ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/reports/house-animation-reference-final/frames.tsv ${CMAKE_SOURCE_DIR}/reports/house-profile-reference/frames.tsv)
 set_tests_properties(house_presentation PROPERTIES TIMEOUT 60)
endif()

if(BUILD_TESTING)
 add_executable(encore_doll_entry_data_tests tests/doll_entry_data_tests.cpp)
 target_link_libraries(encore_doll_entry_data_tests PRIVATE encore_core)
 add_test(NAME doll_entry_data COMMAND encore_doll_entry_data_tests ${CMAKE_SOURCE_DIR}/romfs/data/doll-entry.encbattle)
endif()

if(BUILD_TESTING)
    add_executable(encore_items_data_tests tests/items_data_tests.cpp)
    target_link_libraries(encore_items_data_tests PRIVATE encore_core)
    add_test(NAME items_data COMMAND encore_items_data_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encitems)
    add_executable(encore_items_menu_tests tests/items_menu_tests.cpp)
    target_link_libraries(encore_items_menu_tests PRIVATE encore_core)
    add_test(NAME items_menu COMMAND encore_items_menu_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encitems ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encround)
    add_executable(encore_menu_navigation_tests tests/menu_navigation_tests.cpp)
    target_link_libraries(encore_menu_navigation_tests PRIVATE encore_core)
    add_test(NAME menu_navigation COMMAND encore_menu_navigation_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encitems ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom)
    set_tests_properties(menu_navigation PROPERTIES TIMEOUT 60)
endif()

if(BUILD_TESTING)
    add_executable(encore_doll_round_tests tests/doll_round_tests.cpp)
    target_link_libraries(encore_doll_round_tests PRIVATE encore_core)
    add_test(NAME doll_round COMMAND encore_doll_round_tests ${CMAKE_SOURCE_DIR}/romfs/data/doll-entry.encround ${CMAKE_SOURCE_DIR}/romfs/data/doll-entry.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom)
endif()

if(BUILD_TESTING)
    add_executable(encore_doll_boss_probe tests/doll_boss_probe.cpp)
    target_link_libraries(encore_doll_boss_probe PRIVATE encore_core)
    add_test(NAME doll_boss_reference COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_doll_boss_reference.py --probe $<TARGET_FILE:encore_doll_boss_probe>)
endif()

if(BUILD_TESTING)
    add_executable(encore_house_npc_restore_tests tests/house_npc_restore_tests.cpp)
    target_link_libraries(encore_house_npc_restore_tests PRIVATE encore_core)
    add_test(NAME house_npc_restore COMMAND encore_house_npc_restore_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom ${CMAKE_SOURCE_DIR}/romfs/data/opening.enchouse ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle)
endif()

if(BUILD_TESTING)
 add_executable(encore_world_effect_tests tests/world_effect_tests.cpp)
 target_link_libraries(encore_world_effect_tests PRIVATE encore_core)
 add_test(NAME world_effect COMMAND encore_world_effect_tests ${CMAKE_SOURCE_DIR}/romfs/data/melody.encfx)
 add_test(NAME world_effect_timing COMMAND encore_world_effect_tests --timing ${CMAKE_SOURCE_DIR}/romfs/data/melody.encfx ${CMAKE_SOURCE_DIR}/reports/world-effect-reference/timing.bin)
 add_test(NAME world_effect_compiler COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/world_effect_assets_test.py -v)
 add_test(NAME world_effect_source_reference COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/world_effect_compare_reference.py --probe $<TARGET_FILE:encore_world_effect_tests>)
endif()

if(BUILD_TESTING)
 add_executable(encore_melody_sequence_tests tests/melody_sequence_tests.cpp)
 target_link_libraries(encore_melody_sequence_tests PRIVATE encore_core)
 add_test(NAME melody_sequence COMMAND encore_melody_sequence_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom ${CMAKE_SOURCE_DIR}/romfs/data/opening.enchouse ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle)
endif()

if(BUILD_TESTING)
 add_executable(encore_native_input_tests tests/native_input_tests.cpp)
 target_link_libraries(encore_native_input_tests PRIVATE encore_core)
 add_test(NAME native_input COMMAND encore_native_input_tests ${CMAKE_SOURCE_DIR}/romfs/data/native.encinput)
endif()

if(BUILD_TESTING)
 add_executable(encore_queued_actor_turn_tests tests/queued_actor_turn_tests.cpp)
 target_link_libraries(encore_queued_actor_turn_tests PRIVATE encore_core)
 target_compile_definitions(encore_queued_actor_turn_tests PRIVATE ENCORE_TEST_ROOM_PATH="${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom")
 add_test(NAME queued_actor_turn COMMAND encore_queued_actor_turn_tests)
endif()

if(BUILD_TESTING)
 add_executable(encore_session_save_tests tests/session_save_tests.cpp)
 target_link_libraries(encore_session_save_tests PRIVATE encore_core)
 add_test(NAME session_save COMMAND encore_session_save_tests)
 add_executable(encore_phone_data_tests tests/phone_data_tests.cpp)
 target_link_libraries(encore_phone_data_tests PRIVATE encore_core)
 add_test(NAME phone_data COMMAND encore_phone_data_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encphone)
endif()

if(BUILD_TESTING)
 add_executable(encore_phone_runtime_tests tests/phone_runtime_tests.cpp)
 target_link_libraries(encore_phone_runtime_tests PRIVATE encore_core)
 add_test(NAME phone_runtime COMMAND encore_phone_runtime_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encphone ${CMAKE_SOURCE_DIR}/content/phone-stage/native-animation.bin)
endif()

if(BUILD_TESTING)
 add_executable(encore_carol_phone_sequence_tests tests/carol_phone_sequence_tests.cpp)
 target_link_libraries(encore_carol_phone_sequence_tests PRIVATE encore_core)
 add_test(NAME carol_phone_sequence COMMAND encore_carol_phone_sequence_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom ${CMAKE_SOURCE_DIR}/romfs/data/opening.enchouse ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encphone)
 add_executable(encore_save_menu_tests tests/save_menu_tests.cpp)
 target_link_libraries(encore_save_menu_tests PRIVATE encore_core)
 add_test(NAME save_menu COMMAND encore_save_menu_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encsavemenu)
 add_test(NAME save_menu_assets COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/save_menu_assets_tests.py)
endif()

if(BUILD_TESTING)
 add_executable(encore_dialogue_choices_tests tests/dialogue_choices_tests.cpp)
 target_link_libraries(encore_dialogue_choices_tests PRIVATE encore_core)
 add_test(NAME dialogue_choices COMMAND encore_dialogue_choices_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encchoices)
 add_executable(encore_dialogue_branch_tests tests/dialogue_branch_tests.cpp)
 target_link_libraries(encore_dialogue_branch_tests PRIVATE encore_core)
 add_test(NAME dialogue_branch COMMAND encore_dialogue_branch_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom)
endif()

if(BUILD_TESTING)
 add_executable(encore_dad_record_sequence_tests tests/dad_record_sequence_tests.cpp)
 target_link_libraries(encore_dad_record_sequence_tests PRIVATE encore_core)
 add_test(NAME dad_record_sequence COMMAND encore_dad_record_sequence_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom ${CMAKE_SOURCE_DIR}/romfs/data/opening.enchouse ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encphone ${CMAKE_SOURCE_DIR}/romfs/data/opening.encchoices ${CMAKE_SOURCE_DIR}/romfs/data/opening.encsavemenu)
endif()

if(BUILD_TESTING)
 add_executable(encore_native_session_tests tests/native_session_tests.cpp)
 target_link_libraries(encore_native_session_tests PRIVATE encore_core)
 add_test(NAME native_session COMMAND encore_native_session_tests ${CMAKE_SOURCE_DIR}/romfs/data)
endif()

if(BUILD_TESTING)
 add_executable(encore_house_record_presentation_tests tests/house_record_presentation_tests.cpp)
 target_link_libraries(encore_house_record_presentation_tests PRIVATE encore_core)
 add_test(NAME house_record_fixture COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/house_record_presentation_fixture.py --replay --work ${CMAKE_BINARY_DIR}/house-record-presentation --reports ${CMAKE_SOURCE_DIR}/reports/house-record-presentation)
 set_tests_properties(house_record_fixture PROPERTIES FIXTURES_SETUP house_record_fixture)
 add_test(NAME house_record_presentation COMMAND encore_house_record_presentation_tests ${CMAKE_BINARY_DIR}/house-record-presentation ${CMAKE_SOURCE_DIR}/reports/house-record-presentation ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle)
 set_tests_properties(house_record_presentation PROPERTIES FIXTURES_REQUIRED house_record_fixture)
endif()

if(BUILD_TESTING)
 add_executable(encore_restore_data_tests tests/restore_data_tests.cpp)
 target_link_libraries(encore_restore_data_tests PRIVATE encore_core)
 add_test(NAME restore_data COMMAND encore_restore_data_tests ${CMAKE_SOURCE_DIR}/romfs/data)
 add_executable(encore_session_restore_tests tests/session_restore_tests.cpp)
 target_link_libraries(encore_session_restore_tests PRIVATE encore_core)
 add_test(NAME session_restore COMMAND encore_session_restore_tests ${CMAKE_SOURCE_DIR}/romfs/data)
 add_executable(encore_load_rng_tests tests/load_rng_tests.cpp)
 target_link_libraries(encore_load_rng_tests PRIVATE encore_core)
 add_test(NAME load_rng COMMAND encore_load_rng_tests ${CMAKE_SOURCE_DIR}/reports/load-rng/reference.bin)
 add_executable(encore_slot_preference_tests tests/slot_preference_tests.cpp)
 target_link_libraries(encore_slot_preference_tests PRIVATE encore_core)
 add_test(NAME slot_preference COMMAND encore_slot_preference_tests)
 add_executable(encore_continue_menu_tests tests/continue_menu_tests.cpp)
 target_link_libraries(encore_continue_menu_tests PRIVATE encore_core)
 add_test(NAME continue_menu COMMAND encore_continue_menu_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.enccontinue ${CMAKE_SOURCE_DIR}/romfs/data/opening.encsavemenu ${CMAKE_SOURCE_DIR}/reports/continue-menu/native-fade-oracle.txt)
 add_test(NAME continue_assets COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/continue_assets_tests.py)
 add_executable(encore_fresh_house_restore_tests tests/fresh_house_restore_tests.cpp)
 target_link_libraries(encore_fresh_house_restore_tests PRIVATE encore_core)
 add_test(NAME fresh_house_restore COMMAND encore_fresh_house_restore_tests ${CMAKE_SOURCE_DIR}/romfs/data)
endif()

if(BUILD_TESTING)
 add_executable(encore_load_progress_tests tests/load_progress_tests.cpp)
 target_link_libraries(encore_load_progress_tests PRIVATE encore_core)
 add_test(NAME load_progress COMMAND encore_load_progress_tests)
endif()

if(BUILD_TESTING)
 add_executable(encore_loading_texture_tests tests/loading_texture_tests.cpp)
 target_include_directories(encore_loading_texture_tests BEFORE PRIVATE tests/loading_texture_stubs . include)
 target_link_libraries(encore_loading_texture_tests PRIVATE encore_core)
 add_test(NAME loading_texture COMMAND encore_loading_texture_tests ${CMAKE_BINARY_DIR})
endif()

if(BUILD_TESTING)
 add_executable(encore_loading_indicator_tests tests/loading_indicator_tests.cpp)
 target_include_directories(encore_loading_indicator_tests BEFORE PRIVATE tests/loading_indicator_stubs . include)
 target_link_libraries(encore_loading_indicator_tests PRIVATE encore_core)
 add_test(NAME loading_indicator COMMAND encore_loading_indicator_tests ${CMAKE_SOURCE_DIR}/romfs/)
endif()

if(BUILD_TESTING)
 add_executable(encore_loading_task_plan_tests tests/loading_task_plan_tests.cpp)
 target_include_directories(encore_loading_task_plan_tests PRIVATE include)
 target_link_libraries(encore_loading_task_plan_tests PRIVATE encore_core)
 add_test(NAME loading_task_plan COMMAND encore_loading_task_plan_tests)
endif()

if(BUILD_TESTING)
 if(ENCORE_SANITIZERS)
  add_test(NAME region_background COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test_region_background.py --sanitize)
 else()
  add_test(NAME region_background COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test_region_background.py)
 endif()
 set_tests_properties(region_background PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 180)
endif()

if(BUILD_TESTING)
 add_executable(encore_frame_profile_tests tests/frame_profile_tests.cpp)
 target_link_libraries(encore_frame_profile_tests PRIVATE encore_core)
 add_test(NAME frame_profile COMMAND encore_frame_profile_tests)
endif()

if(BUILD_TESTING)
 add_executable(encore_pillow_sequence_tests tests/pillow_sequence_tests.cpp)
 target_link_libraries(encore_pillow_sequence_tests PRIVATE encore_core)
 add_test(NAME pillow_sequence COMMAND encore_pillow_sequence_tests ${CMAKE_SOURCE_DIR}/romfs/data)
 add_executable(encore_pillow_round_runtime_tests tests/pillow_round_runtime_tests.cpp)
 target_link_libraries(encore_pillow_round_runtime_tests PRIVATE encore_core)
 add_test(NAME pillow_round_runtime COMMAND encore_pillow_round_runtime_tests ${CMAKE_SOURCE_DIR}/romfs/data/pillow-entry.encround ${CMAKE_SOURCE_DIR}/romfs/data/pillow-entry.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/doll-entry.encround ${CMAKE_SOURCE_DIR}/romfs/data/doll-entry.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom)
endif()

if(BUILD_TESTING)
 add_executable(encore_session_migration_tests tests/session_migration_tests.cpp)
 target_link_libraries(encore_session_migration_tests PRIVATE encore_core)
 add_test(NAME session_migration COMMAND encore_session_migration_tests ${CMAKE_SOURCE_DIR}/romfs/data ${CMAKE_SOURCE_DIR}/content/legacy-rules6)
 add_test(NAME session_migration_assets COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/session_migration_assets_tests.py)
endif()

# Exercise the optional GPU span input and unchanged CPU fallback in both host
# and sanitizer aggregate gates. This does not enable the CTR GPU option.
if(BUILD_TESTING)
 add_executable(encore_gpu_direct_region_integrated_tests tests/gpu_direct_region_integrated_host.cpp)
 target_include_directories(encore_gpu_direct_region_integrated_tests PRIVATE ${CMAKE_SOURCE_DIR})
 target_link_libraries(encore_gpu_direct_region_integrated_tests PRIVATE encore_core)
 if(NOT MSVC)
  target_compile_options(encore_gpu_direct_region_integrated_tests PRIVATE -ffp-contract=off)
 endif()
 add_test(NAME gpu_direct_region_integrated COMMAND encore_gpu_direct_region_integrated_tests)
 set_tests_properties(gpu_direct_region_integrated PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 180)
endif()

if(BUILD_TESTING)
 add_executable(encore_gpu_certificate_tests tests/gpu_certificate_host.cpp)
 target_include_directories(encore_gpu_certificate_tests PRIVATE ${CMAKE_SOURCE_DIR})
 target_link_libraries(encore_gpu_certificate_tests PRIVATE encore_core)
 if(NOT MSVC)
  target_compile_options(encore_gpu_certificate_tests PRIVATE -ffp-contract=off)
 endif()
 add_test(NAME gpu_certificates COMMAND encore_gpu_certificate_tests)
 set_tests_properties(gpu_certificates PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 180)
endif()

if(BUILD_TESTING)
 add_executable(encore_persistent_player_tests tests/persistent_player_tests.cpp)
 target_link_libraries(encore_persistent_player_tests PRIVATE encore_core)
 add_test(NAME persistent_player COMMAND encore_persistent_player_tests)
endif()

if(BUILD_TESTING)
    add_executable(encore_new_game_setup_tests tests/new_game_setup_tests.cpp)
    target_link_libraries(encore_new_game_setup_tests PRIVATE encore_core)
    add_test(NAME new_game_setup COMMAND encore_new_game_setup_tests ${CMAKE_SOURCE_DIR}/romfs/data)
    add_test(NAME new_game_source_data COMMAND ${Python3_EXECUTABLE} tools/new_game_assets.py verify)
    set_tests_properties(new_game_setup new_game_source_data PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60)
endif()

# Exact row-linear Pillow span kernel, also exercised under sanitizers.
if(BUILD_TESTING)
 add_executable(encore_gpu_row_linear_tests tests/gpu_row_linear_host.cpp)
 target_include_directories(encore_gpu_row_linear_tests PRIVATE ${CMAKE_SOURCE_DIR})
 target_link_libraries(encore_gpu_row_linear_tests PRIVATE encore_core)
 if(NOT MSVC)
  target_compile_options(encore_gpu_row_linear_tests PRIVATE -ffp-contract=off)
 endif()
 add_test(NAME gpu_row_linear COMMAND encore_gpu_row_linear_tests)
 set_tests_properties(gpu_row_linear PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 180)
endif()

# Startup/music/texture checkpoint. PCM remains source/development-only until
# an actual Podunk scene installs its explicit staging contract.
if(BUILD_TESTING)
 add_executable(encore_gpu_integer_row_tests tests/gpu_integer_row_host.cpp)
 target_include_directories(encore_gpu_integer_row_tests PRIVATE ${CMAKE_SOURCE_DIR})
 target_link_libraries(encore_gpu_integer_row_tests PRIVATE encore_core)
 if(NOT MSVC)
  target_compile_options(encore_gpu_integer_row_tests PRIVATE -ffp-contract=off)
 endif()
 add_test(NAME gpu_integer_rows COMMAND encore_gpu_integer_row_tests)
 set_tests_properties(gpu_integer_rows PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 180)
 add_executable(encore_music_regions_tests tests/music_regions_tests.cpp)
 target_link_libraries(encore_music_regions_tests PRIVATE encore_core)
 add_test(NAME music_regions COMMAND encore_music_regions_tests ${CMAKE_SOURCE_DIR}/romfs/sound/banks/podunk.encmusic ${CMAKE_SOURCE_DIR}/romfs/sound/banks/podunk.encaudio)
 add_executable(encore_music_region_player_tests tests/music_region_player_tests.cpp platform/ctr/music_region_player.cpp)
 target_include_directories(encore_music_region_player_tests BEFORE PRIVATE tests/fixtures/audio_ndsp platform/ctr)
 target_link_libraries(encore_music_region_player_tests PRIVATE encore_core)
 add_test(NAME music_region_player COMMAND encore_music_region_player_tests ${CMAKE_SOURCE_DIR}/romfs/sound/banks/podunk.encmusic ${CMAKE_SOURCE_DIR}/romfs/sound/banks/podunk.encaudio ${CMAKE_SOURCE_DIR}/romfs/)
 foreach(name service prepare)
  add_executable(encore_music_region_${name}_tests tests/music_region_${name}_tests.cpp platform/ctr/audio_player.cpp platform/ctr/music_region_player.cpp platform/ctr/music_region_service.cpp)
  target_include_directories(encore_music_region_${name}_tests BEFORE PRIVATE tests/fixtures/audio_ndsp platform/ctr)
  target_link_libraries(encore_music_region_${name}_tests PRIVATE encore_core)
  target_link_options(encore_music_region_${name}_tests PRIVATE -Wl,--wrap=fopen)
  add_test(NAME music_region_${name} COMMAND encore_music_region_${name}_tests ${CMAKE_SOURCE_DIR}/romfs/sound/banks/opening.encaudio ${CMAKE_SOURCE_DIR}/romfs/ ${CMAKE_SOURCE_DIR}/romfs/sound/banks/podunk.encmusic ${CMAKE_SOURCE_DIR}/romfs/sound/banks/podunk.encaudio ${CMAKE_SOURCE_DIR}/romfs/)
  set_tests_properties(music_region_${name} PROPERTIES TIMEOUT 180)
 endforeach()
 target_link_options(encore_music_region_prepare_tests PRIVATE -Wl,--wrap=fread,--wrap=fclose,--wrap=ferror)
 # Optimized glibc builds can call __fread_chk instead of fread. Detect that
 # actual libc symbol rather than disabling fortification or assuming Linux.
 include(CheckCXXSourceCompiles)
 check_cxx_source_compiles("#include <cstdio>\nextern \"C\" size_t __fread_chk(void*,size_t,size_t,size_t,FILE*);\nint main(){char buffer[1];return int(__fread_chk(buffer,sizeof(buffer),1,1,stdin));}"
   ENCORE_LIBC_HAS_FREAD_CHK)
 if(ENCORE_LIBC_HAS_FREAD_CHK)
  target_compile_definitions(encore_music_region_prepare_tests PRIVATE ENCORE_WRAP_FREAD_CHK=1)
  target_link_options(encore_music_region_prepare_tests PRIVATE -Wl,--wrap=__fread_chk)
  add_executable(encore_music_region_prepare_fortified_tests tests/music_region_prepare_tests.cpp platform/ctr/audio_player.cpp platform/ctr/music_region_player.cpp platform/ctr/music_region_service.cpp)
  target_include_directories(encore_music_region_prepare_fortified_tests BEFORE PRIVATE tests/fixtures/audio_ndsp platform/ctr)
  target_link_libraries(encore_music_region_prepare_fortified_tests PRIVATE encore_core)
  target_compile_definitions(encore_music_region_prepare_fortified_tests PRIVATE ENCORE_WRAP_FREAD_CHK=1)
  target_compile_options(encore_music_region_prepare_fortified_tests PRIVATE -O3 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)
  target_link_options(encore_music_region_prepare_fortified_tests PRIVATE -Wl,--wrap=fopen,--wrap=fread,--wrap=__fread_chk,--wrap=fclose,--wrap=ferror)
  add_test(NAME music_region_prepare_fortified COMMAND encore_music_region_prepare_fortified_tests ${CMAKE_SOURCE_DIR}/romfs/sound/banks/opening.encaudio ${CMAKE_SOURCE_DIR}/romfs/ ${CMAKE_SOURCE_DIR}/romfs/sound/banks/podunk.encmusic ${CMAKE_SOURCE_DIR}/romfs/sound/banks/podunk.encaudio ${CMAKE_SOURCE_DIR}/romfs/)
  set_tests_properties(music_region_prepare_fortified PROPERTIES TIMEOUT 180)
 endif()
 add_executable(encore_audio_player_regression tests/audio_player_regression.cpp platform/ctr/audio_player.cpp)
 target_include_directories(encore_audio_player_regression BEFORE PRIVATE tests/fixtures/audio_ndsp platform/ctr)
 target_link_libraries(encore_audio_player_regression PRIVATE encore_core)
 add_test(NAME audio_player_regression COMMAND encore_audio_player_regression ${CMAKE_SOURCE_DIR}/romfs/sound/banks/opening.encaudio ${CMAKE_SOURCE_DIR}/romfs/ ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom)
 set_tests_properties(audio_player_regression PROPERTIES PASS_REGULAR_EXPRESSION "TRACE 516 56ffdd52e57c7f69")
endif()

if(BUILD_TESTING)
    add_executable(encore_settings_house_prompts_tests tests/settings-house-prompts.cpp)
    target_link_libraries(encore_settings_house_prompts_tests PRIVATE encore_core)
    add_test(NAME settings_house_prompts COMMAND encore_settings_house_prompts_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encprompts ${CMAKE_SOURCE_DIR}/romfs/data/opening.enchouse ${CMAKE_SOURCE_DIR}/romfs/data/opening.encphone ${CMAKE_SOURCE_DIR}/romfs/data/opening.encinspect)
    add_test(NAME settings_source COMMAND ${Python3_EXECUTABLE} tools/startup_settings_assets.py verify)
    add_test(NAME settings_prompt_source COMMAND ${Python3_EXECUTABLE} tools/house_button_prompt_assets.py verify)
    set_tests_properties(settings_source settings_prompt_source PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_custom_target(encore_settings_speed_fixtures COMMAND ${Python3_EXECUTABLE} tests/settings-text-speed.py --prepare-only WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_executable(encore_settings_text_speed_tests tests/settings-text-speed.cpp)
    target_link_libraries(encore_settings_text_speed_tests PRIVATE encore_core)
    add_dependencies(encore_settings_text_speed_tests encore_settings_speed_fixtures)
    add_test(NAME settings_text_speed COMMAND encore_settings_text_speed_tests ${CMAKE_SOURCE_DIR}/build/settings-text-speed ${CMAKE_SOURCE_DIR}/romfs/data/opening.encround ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/reports/battle-round-presentation/native-hp-text.tsv)
    add_executable(encore_settings_palette_tests tests/settings_palette_tests.cpp)
    target_include_directories(encore_settings_palette_tests BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/tests/loading_texture_stubs ${CMAKE_SOURCE_DIR})
    target_link_libraries(encore_settings_palette_tests PRIVATE encore_core)
    add_test(NAME settings_palette COMMAND encore_settings_palette_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encsettings ${CMAKE_SOURCE_DIR}/romfs)
endif()

if(BUILD_TESTING)
 add_executable(encore_localization_tests tests/localization_tests.cpp)
 target_link_libraries(encore_localization_tests PRIVATE encore_core)
 add_test(NAME localization_catalog COMMAND encore_localization_tests ${CMAKE_SOURCE_DIR}/romfs/data/localization.enclocale)
 set_tests_properties(localization_catalog PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 add_executable(encore_localized_slice_tests tests/localized_slice_tests.cpp)
 target_link_libraries(encore_localized_slice_tests PRIVATE encore_core)
 add_test(NAME localized_playable_slice COMMAND encore_localized_slice_tests ${CMAKE_SOURCE_DIR}/romfs/data ${CMAKE_SOURCE_DIR}/romfs/data/localization.enclocale ${CMAKE_SOURCE_DIR}/romfs/fonts/source-fonts.encfont)
 add_test(NAME localization_sources COMMAND ${Python3_EXECUTABLE} tools/localization_assets.py verify)
 set_tests_properties(localization_sources PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# Focused deterministic encounter preparation/admission regressions.
if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(encounter_residency_tests tests/encounter_residency_tests.cpp)
    target_include_directories(encounter_residency_tests PRIVATE include)
    add_test(NAME encounter_residency COMMAND encounter_residency_tests)
    add_executable(load_progress_thread_tests tests/load_progress_thread_tests.cpp)
    target_include_directories(load_progress_thread_tests PRIVATE include)
    target_link_libraries(load_progress_thread_tests PRIVATE Threads::Threads)
    add_test(NAME load_progress_thread COMMAND load_progress_thread_tests)
    add_executable(raw_t3x_tests tests/raw_t3x_tests.cpp)
    target_include_directories(raw_t3x_tests PRIVATE include)
    file(GLOB encounter_t3x_assets "${CMAKE_SOURCE_DIR}/romfs/graphics/battle/lamp/*.t3x" "${CMAKE_SOURCE_DIR}/romfs/graphics/battle/round/*.t3x" "${CMAKE_SOURCE_DIR}/romfs/graphics/battle/doll/*.t3x" "${CMAKE_SOURCE_DIR}/romfs/graphics/battle/pillow/*.t3x")
    add_test(NAME raw_t3x COMMAND raw_t3x_tests ${encounter_t3x_assets})
endif()
if(BUILD_TESTING)
    add_executable(preparation_cancel_tests tests/preparation_cancel_tests.cpp)
    target_include_directories(preparation_cancel_tests PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(preparation_cancel_tests PRIVATE encore_core)
    add_test(NAME preparation_cancel COMMAND preparation_cancel_tests)
    set_tests_properties(preparation_cancel PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    target_link_libraries(encounter_residency_tests PRIVATE encore_core)
    target_link_libraries(load_progress_thread_tests PRIVATE encore_core)
    target_link_libraries(raw_t3x_tests PRIVATE encore_core)
endif()
if(BUILD_TESTING)
    add_executable(preparation_budget_tests tests/preparation_budget_tests.cpp)
    target_link_libraries(preparation_budget_tests PRIVATE encore_core)
    add_test(NAME preparation_budget COMMAND preparation_budget_tests)
endif()
if(BUILD_TESTING)
    add_executable(transition_mask_host tests/transition_mask_host.cpp)
    target_include_directories(transition_mask_host PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME transition_mask_exact COMMAND transition_mask_host)
    set_tests_properties(transition_mask_exact PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME encounter_dependency_manifest COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_encounter_dependencies)
    set_tests_properties(encounter_dependency_manifest PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# These regressions use assert for checked setup as well as verification. Keep
# assertions in their translation units even in Release, without changing the
# optimized production core or its other consumers.
if(BUILD_TESTING)
    foreach(target encore_localization_tests encore_localized_slice_tests
                   encounter_residency_tests load_progress_thread_tests
                   raw_t3x_tests preparation_cancel_tests preparation_budget_tests
                   transition_mask_host)
        if(MSVC)
            target_compile_options(${target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${target} PRIVATE -UNDEBUG)
        endif()
    endforeach()
endif()

if(BUILD_TESTING)
    add_executable(blackbars_tests tests/blackbars_tests.cpp)
    target_include_directories(blackbars_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME blackbars_source COMMAND blackbars_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbars ${CMAKE_SOURCE_DIR}/reports/npc-doorway-occlusion/blackbars-native.tsv)
endif()

if(BUILD_TESTING)
 add_executable(encore_resource_catalog_tests tests/resource_catalog_tests.cpp)
 add_executable(encore_startup_resource_admission_tests tests/startup_resource_admission_tests.cpp)
 target_link_libraries(encore_startup_resource_admission_tests PRIVATE encore_core)
 add_test(NAME startup_resource_bindings COMMAND encore_startup_resource_admission_tests ${CMAKE_SOURCE_DIR}/romfs)
 add_executable(encore_catalog_resource_admission_manual tests/catalog_resource_admission_manual.cpp)
 target_link_libraries(encore_catalog_resource_admission_manual PRIVATE encore_core)
 add_test(NAME catalog_resource_formats COMMAND encore_catalog_resource_admission_manual ${CMAKE_SOURCE_DIR}/romfs/data/native.encresources ${CMAKE_SOURCE_DIR}/romfs)
 add_executable(encore_room_music_admission_tests tests/room_music_admission_tests.cpp)
 target_link_libraries(encore_room_music_admission_tests PRIVATE encore_core)
 add_test(NAME room_music_admission COMMAND encore_room_music_admission_tests ${CMAKE_SOURCE_DIR}/romfs)
 add_executable(encore_field_item_admission_tests tests/field_item_admission_tests.cpp)
 target_link_libraries(encore_field_item_admission_tests PRIVATE encore_core)
 add_test(NAME field_item_admission COMMAND encore_field_item_admission_tests ${CMAKE_SOURCE_DIR}/romfs/data/podunk.encfielditems ${CMAKE_SOURCE_DIR}/romfs/data/opening.encitems ${CMAKE_SOURCE_DIR}/romfs/data/opening.encfield ${CMAKE_SOURCE_DIR}/romfs/data/podunk.encfielddetails)
 target_link_libraries(encore_resource_catalog_tests PRIVATE encore_core)
 add_test(NAME resource_catalog COMMAND encore_resource_catalog_tests ${CMAKE_SOURCE_DIR}/romfs/data/native.encresources ${CMAKE_SOURCE_DIR}/romfs ${CMAKE_BINARY_DIR}/resource-catalog-fixture)
 add_test(NAME resource_catalog_sources COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/resource_catalog_tests.py)
 set_tests_properties(resource_catalog resource_catalog_sources PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_custom_target(encore_settings_preview_fixtures COMMAND ${Python3_EXECUTABLE} tests/settings-preview.py --prepare-only WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 add_executable(encore_settings_preview_tests tests/new_game_settings_preview_tests.cpp)
 target_link_libraries(encore_settings_preview_tests PRIVATE encore_core)
 add_dependencies(encore_settings_preview_tests encore_settings_preview_fixtures)
 add_test(NAME settings_preview COMMAND encore_settings_preview_tests ${CMAKE_SOURCE_DIR}/romfs/data ${CMAKE_SOURCE_DIR}/build/settings-preview)
 add_test(NAME settings_preview_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/settings-preview.py)
 set_tests_properties(settings_preview settings_preview_source PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_test(NAME battle_round_source_bindings COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_battle_round_bindings)
 set_tests_properties(battle_round_source_bindings PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_test(NAME startup_settings_bindings COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/startup-settings-bindings.py)
 set_tests_properties(startup_settings_bindings PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_executable(encore_locale_grammar_tests tests/locale_grammar_tests.cpp)
 target_link_libraries(encore_locale_grammar_tests PRIVATE encore_core)
 add_test(NAME locale_grammar COMMAND encore_locale_grammar_tests ${CMAKE_SOURCE_DIR}/romfs/data/localization.enclocale)
 add_test(NAME locale_grammar_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/locale_grammar_assets_tests.py)
 set_tests_properties(locale_grammar locale_grammar_source PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_custom_target(encore_round_recipe_fixtures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_round_presentation_recipe.py --prepare-only WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 add_executable(encore_round_recipe_tests tests/round_presentation_recipe_tests.cpp)
 target_link_libraries(encore_round_recipe_tests PRIVATE encore_core)
 add_dependencies(encore_round_recipe_tests encore_round_recipe_fixtures)
 add_test(NAME round_recipe_consumer COMMAND encore_round_recipe_tests ${CMAKE_SOURCE_DIR}/build/round-recipe-fixtures/baseline.encround ${CMAKE_SOURCE_DIR}/build/round-recipe-fixtures/changed.encround ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle)
 add_test(NAME round_recipe_source COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_round_presentation_recipe)
 set_tests_properties(round_recipe_consumer round_recipe_source PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_test(NAME boss_presentation_bindings COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_boss_presentation_bindings)
 set_tests_properties(boss_presentation_bindings PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_executable(encore_round_return_recipe_tests tests/round_return_recipe_tests.cpp)
 target_link_libraries(encore_round_return_recipe_tests PRIVATE encore_core)
 add_dependencies(encore_round_return_recipe_tests encore_round_recipe_fixtures)
 add_test(NAME round_return_recipe_consumer COMMAND encore_round_return_recipe_tests ${CMAKE_SOURCE_DIR}/build/round-recipe-fixtures/return-baseline.encround ${CMAKE_SOURCE_DIR}/build/round-recipe-fixtures/return-changed.encround ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle)
 set_tests_properties(round_return_recipe_consumer PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_custom_target(encore_boss_consumer_fixtures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/boss_consumer_fixtures.py --prepare-only WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 add_executable(encore_boss_consumer_tests tests/boss_consumer_tests.cpp)
 target_link_libraries(encore_boss_consumer_tests PRIVATE encore_core)
 add_dependencies(encore_boss_consumer_tests encore_boss_consumer_fixtures)
 add_test(NAME boss_consumer COMMAND encore_boss_consumer_tests ${CMAKE_SOURCE_DIR}/build/boss-consumer-fixtures ${CMAKE_SOURCE_DIR}/romfs/data/doll-entry.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom)
 add_test(NAME boss_consumer_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/boss_consumer_fixtures.py)
 set_tests_properties(boss_consumer boss_consumer_source PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_custom_target(encore_phone_binding_fixtures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_phone_presentation_bindings.py --prepare-only WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 add_dependencies(encore_phone_runtime_tests encore_phone_binding_fixtures)
 add_test(NAME phone_presentation_bindings COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_phone_presentation_bindings)
 add_test(NAME phone_regenerated_consumer COMMAND encore_phone_runtime_tests ${CMAKE_SOURCE_DIR}/build/phone-presentation-fixtures/regenerated.encphone ${CMAKE_SOURCE_DIR}/content/phone-stage/native-animation.bin)
 set_tests_properties(phone_presentation_bindings phone_regenerated_consumer PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_custom_target(encore_items_binding_fixtures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/items_binding_fixtures.py --prepare-only WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 add_executable(encore_items_binding_consumer_tests tests/items_binding_consumer_tests.cpp)
 target_link_libraries(encore_items_binding_consumer_tests PRIVATE encore_core)
 add_dependencies(encore_items_binding_consumer_tests encore_items_binding_fixtures)
 add_test(NAME items_presentation_bindings COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_items_presentation_bindings)
 add_test(NAME items_binding_consumer COMMAND encore_items_binding_consumer_tests ${CMAKE_SOURCE_DIR}/build/items-binding-fixtures ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encround)
 add_test(NAME items_binding_fixtures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/items_binding_fixtures.py)
 set_tests_properties(items_presentation_bindings items_binding_consumer items_binding_fixtures PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_test(NAME world_program_bindings COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_world_program_bindings tests.test_content_pipeline)
 set_tests_properties(world_program_bindings PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_test(NAME programme_lowering_recipe COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_programme_lowering_recipe)
 add_test(NAME battle_entry_bindings COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_battle_entry_bindings)
 set_tests_properties(programme_lowering_recipe battle_entry_bindings PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_test(NAME ui_presentation_bindings COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_ui_presentation_bindings)
 set_tests_properties(ui_presentation_bindings PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_custom_target(encore_entry_binding_fixtures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/battle_entry_bindings.py fixtures --out ${CMAKE_SOURCE_DIR}/build/battle-entry-bindings-fixtures WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 add_executable(encore_entry_binding_consumer_tests tests/battle_entry_binding_consumer_tests.cpp)
 target_link_libraries(encore_entry_binding_consumer_tests PRIVATE encore_core)
 add_dependencies(encore_entry_binding_consumer_tests encore_entry_binding_fixtures)
 add_test(NAME battle_entry_binding_consumer COMMAND encore_entry_binding_consumer_tests ${CMAKE_SOURCE_DIR}/build/battle-entry-bindings-fixtures ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom)
 set_tests_properties(battle_entry_binding_consumer PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_custom_target(encore_house_binding_fixtures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/house_binding_fixtures.py --prepare-only WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 add_executable(encore_house_binding_consumer_tests tests/house_binding_consumer_tests.cpp)
 target_link_libraries(encore_house_binding_consumer_tests PRIVATE encore_core)
 add_dependencies(encore_house_binding_consumer_tests encore_house_binding_fixtures)
 add_test(NAME house_source_bindings COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_house_source_bindings)
 add_test(NAME house_binding_consumer COMMAND encore_house_binding_consumer_tests ${CMAKE_SOURCE_DIR}/build/house-binding-fixtures ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle)
 add_test(NAME house_binding_fixtures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/house_binding_fixtures.py)
 set_tests_properties(house_source_bindings house_binding_consumer house_binding_fixtures PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_custom_target(encore_naming_binding_fixtures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_naming_presentation_bindings.py --prepare-only WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 add_executable(encore_naming_binding_tests tests/naming_presentation_bindings_tests.cpp)
 target_link_libraries(encore_naming_binding_tests PRIVATE encore_core)
 add_dependencies(encore_naming_binding_tests encore_naming_binding_fixtures)
 add_test(NAME naming_presentation_bindings COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_naming_presentation_bindings.py)
 add_test(NAME naming_binding_consumer COMMAND encore_naming_binding_tests ${CMAKE_SOURCE_DIR}/build/naming-bindings-fixtures/default.encnewgame ${CMAKE_SOURCE_DIR}/romfs/data/opening.encsettings)
 set_tests_properties(naming_presentation_bindings naming_binding_consumer PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_test(NAME phone_linker_bindings COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_phone_linker_bindings)
 set_tests_properties(phone_linker_bindings PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_test(NAME pillow_source_bindings COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_pillow_source_bindings)
 set_tests_properties(pillow_source_bindings PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(BUILD_TESTING)
 add_executable(encore_playable_opening_tests tests/playable_opening_tests.cpp)
 target_link_libraries(encore_playable_opening_tests PRIVATE encore_core)
 add_test(NAME playable_opening COMMAND encore_playable_opening_tests ${CMAKE_SOURCE_DIR}/romfs/data)
 set_tests_properties(playable_opening PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 120)
endif()

# These unittest suites do not invoke the current CMake sanitizer binaries.
# Normal GCC/Clang jobs run every method once; CI sanitizer jobs can omit only
# this label while retaining fixture preparers, source CLI and C++ consumers.
if(BUILD_TESTING)
 set_tests_properties(
  python_tools encounter_dependency_manifest battle_round_source_bindings
  round_recipe_source boss_presentation_bindings phone_presentation_bindings
  items_presentation_bindings world_program_bindings programme_lowering_recipe
  battle_entry_bindings ui_presentation_bindings house_source_bindings
  naming_presentation_bindings phone_linker_bindings pillow_source_bindings
  PROPERTIES LABELS python_unit)
endif()

if(BUILD_TESTING)
 add_executable(encore_introduction_tests tests/introduction_tests.cpp)
 target_link_libraries(encore_introduction_tests PRIVATE encore_core)
 add_test(NAME original_introduction COMMAND encore_introduction_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encintro)
 set_tests_properties(original_introduction PROPERTIES TIMEOUT 60)
endif()

# New interaction formats stay in the explicitly requested manual full run.
if(BUILD_TESTING)
    add_executable(encore_house_inspection_data_tests tests/house_inspection_data_tests.cpp)
    target_link_libraries(encore_house_inspection_data_tests PRIVATE encore_core)
    add_test(NAME house_inspection_data COMMAND encore_house_inspection_data_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encinspect)
    add_executable(encore_house_inspection_runtime_tests tests/house_inspection_runtime_tests.cpp)
    target_link_libraries(encore_house_inspection_runtime_tests PRIVATE encore_core)
    add_test(NAME house_inspection_runtime COMMAND encore_house_inspection_runtime_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encinspect ${CMAKE_SOURCE_DIR}/romfs/data/opening.enchouse ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encdrawer)
    add_test(NAME house_inspection_source COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_house_inspections)
    add_test(NAME family_followup_source COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_family_followup)
    set_tests_properties(family_followup_source house_inspection_data house_inspection_runtime house_inspection_source PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60)
endif()

# Apply only after every test is registered, including future additions.
if(BUILD_TESTING)
    add_executable(encore_drawer_program_tests tests/drawer_program_tests.cpp)
    target_link_libraries(encore_drawer_program_tests PRIVATE encore_core)
    add_test(NAME drawer_program COMMAND encore_drawer_program_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.encdrawer ${CMAKE_SOURCE_DIR}/romfs/data/opening.encitems)
    add_test(NAME drawer_program_source COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_drawer_program)
    add_executable(encore_house_drawer_runtime_tests tests/house_drawer_runtime_tests.cpp)
    target_link_libraries(encore_house_drawer_runtime_tests PRIVATE encore_core)
    add_test(NAME house_drawer_runtime COMMAND encore_house_drawer_runtime_tests ${CMAKE_SOURCE_DIR}/romfs/data/opening.enchouse ${CMAKE_SOURCE_DIR}/romfs/data/opening.encroom ${CMAKE_SOURCE_DIR}/romfs/data/opening.encbattle ${CMAKE_SOURCE_DIR}/romfs/data/opening.encinspect ${CMAKE_SOURCE_DIR}/romfs/data/opening.encdrawer)
    set_tests_properties(drawer_program drawer_program_source house_drawer_runtime PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60)
endif()
if(BUILD_TESTING)
    add_executable(encore_storage_runtime_tests tests/storage_runtime_tests.cpp)
    target_link_libraries(encore_storage_runtime_tests PRIVATE encore_core)
    add_test(NAME storage_runtime COMMAND encore_storage_runtime_tests ${CMAKE_SOURCE_DIR}/romfs/data)
    add_test(NAME storage_source COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_native_storage tests.test_storage_dialogue)
    set_tests_properties(storage_runtime storage_source PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60)
endif()
if(BUILD_TESTING)
    add_executable(encore_item_details_tests tests/item_details_tests.cpp)
    target_link_libraries(encore_item_details_tests PRIVATE encore_core)
    add_test(NAME item_details COMMAND encore_item_details_tests ${CMAKE_SOURCE_DIR}/romfs)
    add_test(NAME item_details_source COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_item_details)
    set_tests_properties(item_details item_details_source PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60)
endif()
if(BUILD_TESTING)
    add_executable(encore_item_use_tests tests/item_use_tests.cpp)
    target_link_libraries(encore_item_use_tests PRIVATE encore_core)
    add_test(NAME item_use COMMAND encore_item_use_tests ${CMAKE_SOURCE_DIR}/romfs)
    add_executable(encore_field_equipment_tests tests/field_equipment_tests.cpp)
    target_link_libraries(encore_field_equipment_tests PRIVATE encore_core)
    add_test(NAME field_equipment COMMAND encore_field_equipment_tests ${CMAKE_SOURCE_DIR}/romfs)
    add_test(NAME field_equipment_source COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_field_equipment tests.test_field_audio)
    set_tests_properties(field_equipment field_equipment_source PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 120)
endif()
if(BUILD_TESTING)
    add_executable(encore_certified_texture_background_tests tests/certified_texture_background_tests.cpp)
    target_link_libraries(encore_certified_texture_background_tests PRIVATE encore_core)
    target_compile_options(encore_certified_texture_background_tests PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-ffp-contract=off>)
    add_test(NAME certified_texture_background COMMAND encore_certified_texture_background_tests)
    set_tests_properties(certified_texture_background PROPERTIES TIMEOUT 120)
endif()
# Runs only as part of the explicitly requested manual test suite.
if(BUILD_TESTING)
    add_executable(encore_global_yaml_caches_manual tests/global_yaml_caches_manual.cpp)
    target_link_libraries(encore_global_yaml_caches_manual PRIVATE encore_core)
    add_executable(encore_global_item_definitions_manual tests/global_item_definitions_manual.cpp)
    target_link_libraries(encore_global_item_definitions_manual PRIVATE encore_core)
    add_test(NAME global_item_definitions_manual
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/global_item_definitions_manual.py
            --loader $<TARGET_FILE:encore_global_item_definitions_manual>
            --global-pack ${CMAKE_SOURCE_DIR}/romfs/data/global.encfielditems
            --scoped-pack ${CMAKE_SOURCE_DIR}/romfs/data/podunk.encfielditems
            --work ${CMAKE_BINARY_DIR}/global-items-manual)
    set_tests_properties(global_item_definitions_manual PROPERTIES TIMEOUT 60)
endif()

include(cmake/TestParallelism.cmake)

# Resource writers finish before compilation or fixture preparers read packs.
if(ENCORE_REGENERATE_NATIVE_CONTENT)
    get_property(encore_build_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(encore_target IN LISTS encore_build_targets)
        if(NOT encore_target STREQUAL "encore_native_content")
            add_dependencies(${encore_target} encore_native_content)
        endif()
    endforeach()
endif()
