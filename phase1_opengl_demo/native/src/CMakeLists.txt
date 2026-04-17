cmake_minimum_required(VERSION 3.18.1)

project("maplibre_ohos")

# ============================================================
# 编译选项
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android/OpenHarmony NDK 风格：隐藏符号，-fPIC
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# ============================================================
# 源文件
# ============================================================
add_library(maplibre_ohos SHARED
    src/main.cpp
    src/map_renderer.cpp
    src/tile_loader.cpp
    src/coordinate.cpp
)

# ============================================================
# 头文件搜索路径
# ============================================================
target_include_directories(maplibre_ohos PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================
# OpenGL ES 2.0
# ============================================================
find_library(log-lib           log)
find_library(EGL-lib           EGL)
find_library(GLESv2-lib        GLESv2)

if(NOT EGL-lib)
    message(WARNING "EGL library not found, using system defaults")
endif()

target_link_libraries(maplibre_ohos
    ${log-lib}
    ${EGL-lib}
    ${GLESv2-lib}
)

# ============================================================
# libcurl（瓦片下载）
# ============================================================
find_library(curl-lib curl)

if(curl-lib)
    target_link_libraries(maplibre_ohos ${curl-lib})
else()
    # OpenHarmony NDK 中可能没有预编译的 libcurl
    # 需要用户自行下载并配置，或使用 hdc 工具链中的版本
    message(WARNING "libcurl not found. Tile loading will be disabled.")
    target_compile_definitions(maplibre_ohos PRIVATE NO_CURL)
endif()

# ============================================================
# OpenHarmony NDK 特定配置
# ============================================================
# 使用 @ohos.NDK@ 工具链风格
set(CMAKE_SYSROOT "$ENV{OH_SDK_ROOT}/native")
set(CMAKE_FIND_ROOT_PATH "$ENV{OH_SDK_ROOT}/native/sysroot")

# 告诉 CMake 搜索 NDK sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# ============================================================
# 着色器文件（嵌入为字符串，CMake 负责读取）
# ============================================================
# 着色器源码在 cpp 文件中以字符串常量形式包含（见 map_renderer.cpp）
# 如需外部文件加载，可启用以下选项：
# target_compile_options(maplibre_ohos PRIVATE -DSHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")

# ============================================================
# 输出配置
# ============================================================
# 设置 .so 输出目录
set_target_properties(maplibre_ohos PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/../entry/src/main/resources/rawfile
    RUNTIME_OUTPUT_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/../entry/src/main/resources/rawfile
)

# ============================================================
# 安装规则（可选）
# ============================================================
install(TARGETS maplibre_ohos
    LIBRARY DESTINATION lib/${OHOS_ARCH}
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/map_renderer.h
              ${CMAKE_CURRENT_SOURCE_DIR}/src/tile_loader.h
              ${CMAKE_CURRENT_SOURCE_DIR}/src/coordinate.h
    DESTINATION include/
)
