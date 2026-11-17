add_executable(rl_tests
    main.cpp
    test_i18n.cpp
    test_settings.cpp
    test_core.cpp
    test_parsers.cpp
    test_accounts.cpp
    test_games.cpp
    test_locator.cpp
)
target_link_libraries(rl_tests PRIVATE rl_core doctest)
target_compile_definitions(rl_tests PRIVATE RL_LOCALES_DIR="${CMAKE_SOURCE_DIR}/locales")
rl_set_warnings(rl_tests)
if(MINGW)
    target_link_options(rl_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_test(NAME rl_tests COMMAND rl_tests)
