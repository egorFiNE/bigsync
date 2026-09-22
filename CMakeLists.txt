cmake_minimum_required(VERSION 3.20)

project(bigsync VERSION 0.5.0 LANGUAGES C)

include(GNUInstallDirs)
include(CTest)

add_compile_definitions(_FILE_OFFSET_BITS=64)

if(APPLE)
    add_compile_definitions(_DARWIN_FEATURE_64_BIT_INODE)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall "$<$<CONFIG:Release>:-funroll-loops>")
endif()

add_executable(bigsync bigsync.c md4.c hr.c)
target_compile_definitions(bigsync PRIVATE VERSION="${PROJECT_VERSION}")

add_custom_target(dev DEPENDS bigsync)

if(BUILD_TESTING)
    add_executable(bigsync_test test.c md4.c)
    add_dependencies(bigsync_test bigsync)

    add_test(NAME bigsync COMMAND bigsync_test)
    set_tests_properties(bigsync PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:bigsync>"
        FAIL_REGULAR_EXPRESSION "FAIL;Some tests failed"
    )
endif()

install(TARGETS bigsync RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES bigsync.1 DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")

set(dist_name "${PROJECT_NAME}-${PROJECT_VERSION}")
set(dist_sources
    bigsync.c
    hr.c
    hr.h
    md4.c
    md4.h
    md4_global.h
    test.c
    bigsync.1
    CMakeLists.txt
    README.md
    LICENSE
)
list(TRANSFORM dist_sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

add_custom_target(dist
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${dist_name}"
    COMMAND "${CMAKE_COMMAND}" -E copy ${dist_sources} "${dist_name}/"
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${dist_name}.tar.gz" "${dist_name}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM
)
