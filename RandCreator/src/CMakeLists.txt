project(RandCreator)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( RandCreator RandCreator RandCreator.cpp )
target_include_directories( RandCreator PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( RandCreator ${CMAKE_SOURCE_DIR}/../ricardian )