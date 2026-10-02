# Included by modules/CMakeLists.txt. Registers module includes and tests with unit_tests.
target_include_directories(modules PUBLIC
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/include"
)

set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_INCLUDES
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/include"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src")

set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_SOURCES
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/ProgressionLayout.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/InstanceProfile.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/ContentPackRegistry.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/ItemBudgetScaler.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/CoAContentScalingConfig.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/AdaptiveEncounterAPI.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/encounters/EncounterAdapterRegistry.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/encounters/classic/RazorgoreAdapter.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/encounters/classic/TwinEmperorsAdapter.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/encounters/classic/ChessEventAdapter.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/encounters/wotlk/FourHorsemenAdapter.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/encounters/wotlk/FlameLeviathanAdapter.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/encounters/wotlk/ValithriaAdapter.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/src/encounters/wotlk/LichKingAdapter.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/tests/ProgressionLayoutTest.cpp"
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/tests/EncounterAdaptationTest.cpp")

# The expansion content packs ship inside this module rather than as modules of their own, so
# there is one copy of each and the loader below calls them directly.
target_include_directories(modules PUBLIC
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/content-packs/mod-coa-tbc-content/src"
)

target_sources(modules PRIVATE
    "${CMAKE_SOURCE_DIR}/modules/mod-coa-content-scaling/content-packs/mod-coa-tbc-content/src/CoATBCContent.cpp"
)
