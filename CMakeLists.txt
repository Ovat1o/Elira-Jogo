cmake_minimum_required(VERSION 3.22)

project(ELIRA VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ELIRA_USE_SYSTEM_RAYLIB "Usar uma instalacao local da raylib" OFF)

# Inclui o runtime do Visual C++ no executavel para o jogador nao precisar
# instalar o Microsoft Visual C++ Redistributable separadamente.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(ELIRA_USE_SYSTEM_RAYLIB)
    find_package(raylib 6.0 CONFIG REQUIRED)
else()
    include(FetchContent)

    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    # Mantem o build reproduzivel na mesma versao usada pelos headers do projeto.
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 6.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

if(APPLE)
    add_executable(ELIRA MACOSX_BUNDLE
        src/main.c
        src/funcoes.c
    )
else()
    add_executable(ELIRA
        src/main.c
        src/funcoes.c
    )
endif()

target_include_directories(ELIRA PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/raylib"
)

target_link_libraries(ELIRA PRIVATE raylib)

if(MSVC)
    target_compile_options(ELIRA PRIVATE /W4 /utf-8)
else()
    target_compile_options(ELIRA PRIVATE -Wall -Wextra)
endif()

# Evita que geradores multi-configuracao criem build/bin/Release/ELIRA.exe.
set_target_properties(ELIRA PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)

if(APPLE)
    set_target_properties(ELIRA PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "ELIRA"
        MACOSX_BUNDLE_GUI_IDENTIFIER "br.edu.elira.game"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
    target_link_libraries(ELIRA PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework OpenGL"
        "-framework CoreVideo"
    )
endif()

# Cada plataforma recebe um layout instalavel com todos os recursos do jogo.
if(WIN32)
    install(TARGETS ELIRA RUNTIME DESTINATION .)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION .)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LEIA-ME-RELEASE.txt"
            DESTINATION . RENAME "LEIA-ME.txt")
elseif(APPLE)
    install(TARGETS ELIRA BUNDLE DESTINATION .)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            DESTINATION "ELIRA.app/Contents/Resources")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LEIA-ME-RELEASE.txt"
            DESTINATION "ELIRA.app/Contents/Resources" RENAME "LEIA-ME.txt")
elseif(UNIX)
    install(TARGETS ELIRA RUNTIME DESTINATION "lib/elira")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "lib/elira")
    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/elira-launcher"
            DESTINATION "bin" RENAME "elira")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/elira.desktop"
            DESTINATION "share/applications")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/elira_sorrindo.png"
            DESTINATION "share/pixmaps" RENAME "elira.png")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LEIA-ME-RELEASE.txt"
            DESTINATION "share/doc/elira" RENAME "LEIA-ME.txt")
endif()

set(ELIRA_PACKAGE_SUFFIX "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
    CACHE STRING "Sufixo do instalador gerado")
set(ELIRA_PACKAGE_VERSION "${PROJECT_VERSION}"
    CACHE STRING "Versao gravada no instalador")
set(CPACK_PACKAGE_NAME "ELIRA")
set(CPACK_PACKAGE_VENDOR "Equipe ELIRA")
set(CPACK_PACKAGE_VERSION "${ELIRA_PACKAGE_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Jogo de ficcao cientifica sobre letramento digital e inteligencia artificial")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_FILE_NAME "ELIRA-${ELIRA_PACKAGE_SUFFIX}-Setup")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "ELIRA")
    set(CPACK_NSIS_DISPLAY_NAME "ELIRA - Escape Run Temporal")
    set(CPACK_NSIS_PACKAGE_NAME "ELIRA")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "ELIRA.exe")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$DESKTOP\\\\ELIRA.lnk' '$INSTDIR\\\\ELIRA.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$DESKTOP\\\\ELIRA.lnk'")
elseif(APPLE)
    set(CPACK_GENERATOR "productbuild")
    set(CPACK_PACKAGE_FILE_NAME "ELIRA-${ELIRA_PACKAGE_SUFFIX}")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_FILE_NAME "ELIRA-${ELIRA_PACKAGE_SUFFIX}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_NAME "elira")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Equipe ELIRA")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libc6, libasound2, libgl1, libx11-6, libxrandr2, libxi6, libxcursor1, libxinerama1")
endif()

include(CPack)
