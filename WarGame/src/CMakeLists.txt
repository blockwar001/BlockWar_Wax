project(WarGame)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( WarGame WarGame WarGame.cpp )
target_include_directories( WarGame PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( WarGame ${CMAKE_SOURCE_DIR}/../ricardian )