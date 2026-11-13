# Native tests: build the add-on sources without the Archicad API

add_executable(test_search_engine_native
    test_search_engine_native.cpp
    ../addon/src/SearchEngine.cpp
    ../addon/src/Settings.cpp
)
target_include_directories(test_search_engine_native PRIVATE ../addon/include)
add_test(NAME SearchEngineNative COMMAND test_search_engine_native)

# Spacebar hook edge cases, run through a real Win32 message queue. The stub
# header records synthesized keystrokes instead of typing them.
add_executable(test_message_hook
    test_message_hook.cpp
    ${BAAZ_SHARED_SOURCES}
)
target_include_directories(test_message_hook PRIVATE ../addon/include)
target_compile_definitions(test_message_hook PRIVATE UNICODE _UNICODE)
target_compile_options(test_message_hook PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/test_sendinput_stub.hpp")
target_link_libraries(test_message_hook PRIVATE ${BAAZ_WIN32_LIBS})
add_test(NAME MessageHookEdgeCases COMMAND test_message_hook)

# Hint texts of the command line; no Win32 calls
add_executable(test_command_hint
    test_command_hint.cpp
    ../addon/src/CommandHint.cpp
)
target_include_directories(test_command_hint PRIVATE ../addon/include)
target_compile_definitions(test_command_hint PRIVATE UNICODE _UNICODE)
add_test(NAME CommandHint COMMAND test_command_hint)

# Filter and labels for the rows of Archicad's command table; no Archicad API
add_executable(test_command_catalog
    test_command_catalog.cpp
    ../addon/src/CommandCatalog.cpp
)
target_include_directories(test_command_catalog PRIVATE ../addon/include)
target_compile_definitions(test_command_catalog PRIVATE UNICODE _UNICODE)
add_test(NAME CommandCatalog COMMAND test_command_catalog)
