project(RollGame)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( RollGame RollGame RollGame.cpp )
target_include_directories( RollGame PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( RollGame ${CMAKE_SOURCE_DIR}/../ricardian )