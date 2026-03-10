# ==============================================================================
# 1. 核心配置：编译器路径强制指定 (解决 Windows 空格和 Clangd 问题)
# ==============================================================================
cmake_minimum_required(VERSION 3.25)

# ⚠️ 注意：这里直接锁死 D 盘无空格路径，防止 CMake 自动去 C 盘找带空格的路径
# 如果以后移动了编译器，只需改这两行
set(CMAKE_C_COMPILER "D:/armGCC/bin/arm-none-eabi-gcc.exe")
set(CMAKE_CXX_COMPILER "D:/armGCC/bin/arm-none-eabi-g++.exe")
set(CMAKE_ASM_COMPILER "D:/armGCC/bin/arm-none-eabi-gcc.exe")

# ==============================================================================
# 2. 系统设置 (RASC 标准配置)
# ==============================================================================
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm) 
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 定义构建类型 (Debug/Release)
set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo MinSizeRel)

# 处理多配置生成器逻辑
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT isMultiConfig)
    if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED BUILD_TYPE)
        message(NOTICE "Defaulting to Debug build")
        set(CMAKE_BUILD_TYPE Debug)
    elseif(DEFINED BUILD_TYPE)
        set(CMAKE_BUILD_TYPE ${BUILD_TYPE})
    endif()
endif()

# ==============================================================================
# 3. 项目定义
# ==============================================================================
project(cmake_test
    VERSION 1.0.0
    LANGUAGES C CXX ASM
)

# 【重要修改】此处强制定义 C++ 标准变量，优先级高于 set_property
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 【重要修改】暴力注入 -std=c++17 参数，确保 constexpr if 能通过编译
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)

set(CMAKE_CROSSCOMPILING "TRUE")
set(CMAKE_EXPORT_COMPILE_COMMANDS "TRUE") # 生成供 Clangd 使用的数据库

# ==============================================================================
# 4. 导入 RASC 生成的配置 (不要动这里)
# ==============================================================================
include(${CMAKE_CURRENT_LIST_DIR}/Config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/GeneratedCfg.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/GeneratedSrc.cmake)



file(GLOB_RECURSE USER_SOURCES 
    "src/*.c"
    "src/*.cpp"
    "src/*.S"
    "Driver/*.c"
    "Application/*.cpp"
    "Application/*.c"
    "Driver/*.cpp"
   
   
) 




target_include_directories(${PROJECT_NAME}.elf PRIVATE 
    "src"
    "src/UART"
    "src/DMC"
    "src/SPI"
    "src/GPIO"
    "src/ELC"
    "src/IRQ"
    "src/QSPI"
    "src/TIMER"
    "src/I2C"
    "Driver"
    "src/ADC"
    "DrvBase"
    "Application"
    "NameSpace"
    "ra/arm/CMSIS-DSP/Include"
    "Algorithm"
    "src/DAC"
    

   
    
    # 如果你有其他存放头文件的文件夹，加在这里，例如 "my_libs/include"
)

# ==============================================================================
# 6. 目标构建与链接
# ==============================================================================
set(TARGET_NAME ${PROJECT_NAME}.elf)

# 将 RASC 生成的源文件 + 用户扫描到的源文件 加入工程
target_sources(${TARGET_NAME} PRIVATE 
    ${USER_SOURCES}
)

# 链接库 (数学库 m, 必须加)
target_link_libraries(${TARGET_NAME} PRIVATE m)

# 链接选项
target_link_options(${TARGET_NAME} PRIVATE 
     "--specs=nano.specs"
      "-Wl,--print-memory-usage"
      "-Wl,-Map=${PROJECT_NAME}.map"
      "-u _printf_float"
      
      
)

# ==============================================================================
# 7. 编译标准与优化 (C/C++ 混编配置)
# ==============================================================================

# C 语言标准 (C11)
set_property(TARGET ${TARGET_NAME} PROPERTY C_STANDARD 11)
set_property(TARGET ${TARGET_NAME} PROPERTY C_STANDARD_REQUIRED ON)

# C++ 语言标准 (C++17)
# 虽然上面加了暴力注入，这里的 Property 设置依然保留作为双重保险
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_EXTENSIONS OFF)

file(GLOB_RECURSE CMSIS_DSP_SOURCES "ra/arm/CMSIS-DSP/Source/*.c")
set_source_files_properties(${CMSIS_DSP_SOURCES} PROPERTIES COMPILE_FLAGS "-O0")

# 🔒 嵌入式 C++ 关键优化 (防止内存爆炸)
# 针对所有 C++ 文件自动添加这些 flag
target_compile_options(${TARGET_NAME} PRIVATE
    # --------------------------------------------------------
    # 1. C++ 专属选项 (不要让 C 编译器看到这些，否则报错)
    # --------------------------------------------------------
    $<$<COMPILE_LANGUAGE:CXX>:
        -fno-exceptions          
        -fno-rtti                
        -fno-use-cxa-atexit      
        -fno-threadsafe-statics  
        -Wno-register            
        -Wno-write-strings    
        -Wno-aggregate-return   
    >
    -Wno-aggregate-return 
    -Wno-conversion          # 屏蔽类型截断警告 (修复 q63_t -> q31_t 刷屏)
    -Wno-float-equal         # 屏蔽浮点数比较警告 (修复 pivot != 0.0f 刷屏)
    -Wno-extra
    -Wno-sign-conversion     # 屏蔽 utils.h 里的那堆警告
    -O2                      # ⚠️ 必须放在外面！让 C 文件的编译也强制关闭优化
    -g                       # 生成调试信息
)
target_compile_definitions(${TARGET_NAME} PRIVATE
    __RA6M5__                   # 定义芯片系列
    __DCACHE_PRESENT=1          # ✅ 告诉 CMSIS：我有数据缓存！(解锁 SCB_DisableDCache)
    __ICACHE_PRESENT=1          # 告诉 CMSIS：我有指令缓存！
    __FPU_PRESENT=1             # 告诉 CMSIS：我有浮点运算单元！
    __DSP_PRESENT=1             # Cortex-M33 DSP 指令集
    ARM_MATH_CM33               # ARM 数学库需要
)