cmake_minimum_required(VERSION 3.10)
project(SoundBrick VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find SDL2 and libcurl using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(CURL REQUIRED libcurl)

# Executable target
add_executable(soundbrick 
    src/main.cpp
    src/network/HttpClient.cpp
    src/soundcloud/SoundCloudClient.cpp
    src/ui/TextRenderer.cpp
)

# Link SDL2 and Curl
target_link_libraries(soundbrick PRIVATE ${SDL2_LIBRARIES} ${CURL_LIBRARIES})

# Include directories
target_include_directories(soundbrick PRIVATE third_party)
if(SDL2_INCLUDE_DIRS)
    target_include_directories(soundbrick PRIVATE ${SDL2_INCLUDE_DIRS})
endif()
if(CURL_INCLUDE_DIRS)
    target_include_directories(soundbrick PRIVATE ${CURL_INCLUDE_DIRS})
endif()
# Add library paths from pkg-config
if(SDL2_LIBRARY_DIRS)
    target_link_directories(soundbrick PRIVATE ${SDL2_LIBRARY_DIRS})
endif()
if(CURL_LIBRARY_DIRS)
    target_link_directories(soundbrick PRIVATE ${CURL_LIBRARY_DIRS})
endif()

# Installation rules
install(TARGETS soundbrick DESTINATION bin)
