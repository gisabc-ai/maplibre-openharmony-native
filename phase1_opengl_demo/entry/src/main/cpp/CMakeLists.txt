cmake_minimum_required(VERSION 3.18.1)

project("maplibre_ohos")

# ============================================================
# 编译选项
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# ============================================================
# 源文件（相对于 CMakeLists.txt 所在目录）
# ============================================================
add_library(maplibre_ohos SHARED
    main.cpp
    map_renderer.cpp
    tile_loader.cpp
    coordinate.cpp
)

# ============================================================
# 头文件搜索路径
# ============================================================
target_include_directories(maplibre_ohos PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================
# OpenHarmony NDK - 系统库
# ============================================================
find_library(log-lib     NAMES log)
find_library(EGL-lib     NAMES EGL)
find_library(GLESv2-lib NAMES GLESv2)
find_library(curl-lib   NAMES curl)

target_link_libraries(maplibre_ohos
    ${log-lib}
    ${EGL-lib}
    ${GLESv2-lib}
    ${curl-lib}
)

# ============================================================
# OpenHarmony NDK sysroot 配置
# ============================================================
if(DEFINED ENV{OH_SDK_ROOT})
    set(CMAKE_SYSROOT "$ENV{OH_SDK_ROOT}/native")
    set(CMAKE_FIND_ROOT_PATH "$ENV{OH_SDK_ROOT}/native/sysroot")
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    message(STATUS "OH_SDK_ROOT: $ENV{OH_SDK_ROOT}")
endif()

# ============================================================
# 输出目录
# ============================================================
set_target_properties(maplibre_ohos PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${OHOS_ARCH}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${OHOS_ARCH}
)
