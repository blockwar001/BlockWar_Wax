project(RaceGame)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( RaceGame RaceGame RaceGame.cpp )
target_include_directories( RaceGame PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( RaceGame ${CMAKE_SOURCE_DIR}/../ricardian )