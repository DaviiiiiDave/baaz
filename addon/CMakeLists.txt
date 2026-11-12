# Baaz.apx, the Archicad add-on (version: BAAZ_AC_VERSION)

set(DEVKIT "${ARCHICAD_DEVKIT_DIR}/Support")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# --- Resources -----------------------------------------------------------------
# The .grc files are the source. tools/CompileResources.py runs the DevKit's
# ResConv on them and compiles RFIX.win/AddOnMain.rc2 into Baaz.res.

# Local test builds can take a different MDID from resources/AddOnFix.local.grc
# (not committed). Never distribute such a build.
option(BAAZ_DEV_MDID "Use the MDID in resources/AddOnFix.local.grc (local testing only)" OFF)

set(RES_STAGE ${CMAKE_CURRENT_BINARY_DIR}/ResourceSources)
set(RES_OBJECTS ${CMAKE_CURRENT_BINARY_DIR}/ResourceObjects)
set(RES_FILE ${RES_OBJECTS}/Baaz.res)
set(RES_STAMP ${RES_OBJECTS}/Baaz.res.stamp)
file(GLOB RES_SOURCES CONFIGURE_DEPENDS resources/RINT/*.grc resources/RFIX/*.grc resources/RFIX.win/*.rc2)

set(RES_STAGE_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${RES_STAGE})
if(BAAZ_DEV_MDID)
    set(LOCAL_MDID ${CMAKE_CURRENT_SOURCE_DIR}/resources/AddOnFix.local.grc)
    if(NOT EXISTS ${LOCAL_MDID})
        message(FATAL_ERROR "BAAZ_DEV_MDID is ON but ${LOCAL_MDID} does not exist")
    endif()
    message(WARNING "BAAZ_DEV_MDID is ON: building with a borrowed MDID - local testing only, do not distribute")
    list(APPEND RES_SOURCES ${LOCAL_MDID})
    list(APPEND RES_STAGE_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy ${LOCAL_MDID} ${RES_STAGE}/RFIX/AddOnFix.grc)
endif()

add_custom_command(
    OUTPUT ${RES_STAMP}
    ${RES_STAGE_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RES_OBJECTS}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/CompileResources.py
            INT ${BAAZ_AC_VERSION} ${ARCHICAD_DEVKIT_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src ${RES_STAGE} ${RES_OBJECTS} ${RES_FILE}
    COMMAND ${CMAKE_COMMAND} -E touch ${RES_STAMP}
    DEPENDS ${RES_SOURCES} ${CMAKE_SOURCE_DIR}/tools/CompileResources.py
    COMMENT "Compiling Archicad resources"
)

# --- Add-on --------------------------------------------------------------------

file(GLOB BAAZ_HEADERS CONFIGURE_DEPENDS include/*.hpp include/*.h)

add_library(Baaz SHARED
    src/BaazAddon.cpp
    ${BAAZ_SHARED_SOURCES}
    ${BAAZ_HEADERS}
    ${RES_STAMP}
)

set_target_properties(Baaz PROPERTIES SUFFIX ".apx")

file(GLOB DEVKIT_MODULE_DIRS "${DEVKIT}/Modules/*")
file(GLOB DEVKIT_MODULE_LIBS "${DEVKIT}/Modules/*/*/*.lib")

target_include_directories(Baaz PRIVATE include)
target_include_directories(Baaz SYSTEM PRIVATE "${DEVKIT}/Inc" ${DEVKIT_MODULE_DIRS})

target_compile_definitions(Baaz PRIVATE
    UNICODE
    _UNICODE
    _ITERATOR_DEBUG_LEVEL=0
    ACExtension
    WINDOWS
    ACAPI_VERSION=${BAAZ_AC_VERSION}
)

target_link_libraries(Baaz PRIVATE
    "${DEVKIT}/Lib/ACAP_STAT.lib"
    ${DEVKIT_MODULE_LIBS}
    ${BAAZ_WIN32_LIBS}
)

target_link_options(Baaz PRIVATE
    ${RES_FILE}
    /export:GetExportedFuncAddrs,@1
    /export:SetImportedFuncAddrs,@2
)
