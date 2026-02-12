cmake_minimum_required(VERSION 3.16)
project(glex)

set(CMAKE_CXX_STANDARD 17)

# ============================================================
# GLEX - OpenGL ES Rendering Framework for OpenHarmony
# ============================================================

# 核心源文件
set(GLEX_CORE_SOURCES
    src/glex/GLContext.cpp
    src/glex/ShaderProgram.cpp
    src/glex/RenderPipeline.cpp
    src/glex/RenderThread.cpp
)

# NAPI 桥接层源文件
set(GLEX_BRIDGE_SOURCES
    src/bridge/GLEXBridge.cpp
    src/bridge/DemoPass.cpp
)

# 头文件搜索路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge
)

# 构建共享库
add_library(glex SHARED
    ${GLEX_CORE_SOURCES}
    ${GLEX_BRIDGE_SOURCES}
)

# 平台特定配置
if(OHOS)
    add_definitions(-DOHOS_PLATFORM)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

# 链接系统库
target_link_libraries(glex
    PUBLIC
    libace_ndk.z.so
    libace_napi.z.so
    libGLESv3.so
    libEGL.so
    libhilog_ndk.z.so
    libnative_window.so
)
