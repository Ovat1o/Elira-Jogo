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

add_executable(ELIRA
    src/main.c
    src/funcoes.c
)

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
    target_link_libraries(ELIRA PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework OpenGL"
        "-framework CoreVideo"
    )
endif()

# Layout portavel usado pelos pacotes publicados no GitHub Releases.
install(TARGETS ELIRA RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION .)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LEIA-ME-RELEASE.txt"
        DESTINATION . RENAME "LEIA-ME.txt")

if(APPLE)
    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/JOGAR.command" DESTINATION .)
elseif(UNIX)
    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/JOGAR.sh" DESTINATION .)
endif()
