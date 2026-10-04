# A bounded CI schedule. Unreviewed tests and timing probes remain exclusive.
# Reviewed source suites use copies/unique temp dirs; fixture preparation has
# already finished during the build. Re-review IO if these tests are changed.
if(BUILD_TESTING AND ENCORE_TEST_PARALLEL)
  set(ENCORE_TEST_JOBS 4 CACHE STRING "Reviewed test subprocess limit")
  if(NOT ENCORE_TEST_JOBS MATCHES "^[0-9]+$" OR ENCORE_TEST_JOBS LESS 4 OR ENCORE_TEST_JOBS GREATER 64)
    message(FATAL_ERROR "ENCORE_TEST_JOBS must be between 4 and 64")
  endif()
  get_property(encore_registered_tests DIRECTORY PROPERTY TESTS)
  set_tests_properties(${encore_registered_tests} PROPERTIES RUN_SERIAL TRUE)
  set(encore_parallel_tests
    room_data battle_data audio_data native_session new_game_setup
    resource_catalog playable_opening original_introduction
    encounter_dependency_manifest battle_round_source_bindings round_recipe_source
    boss_presentation_bindings phone_presentation_bindings items_presentation_bindings
    world_program_bindings programme_lowering_recipe battle_entry_bindings
    ui_presentation_bindings house_source_bindings naming_presentation_bindings
    phone_linker_bindings pillow_source_bindings)
  foreach(encore_test IN LISTS encore_parallel_tests)
    if(encore_test IN_LIST encore_registered_tests)
      set_tests_properties(${encore_test} PROPERTIES RUN_SERIAL FALSE)
    endif()
  endforeach()
  if(python_tools IN_LIST encore_registered_tests)
    set_tests_properties(python_tools PROPERTIES RUN_SERIAL FALSE PROCESSORS ${ENCORE_TEST_JOBS})
    set_property(TEST python_tools APPEND PROPERTY ENVIRONMENT ENCORE_PYTHON_TEST_JOBS=${ENCORE_TEST_JOBS})
  endif()
endif()
