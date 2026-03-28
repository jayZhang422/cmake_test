# ==============================================================================
# 💻 自动识别电脑主机名并配置 RASC 路径
# ==============================================================================

# 获取当前电脑的主机名
cmake_host_system_information(RESULT CURRENT_PC_NAME QUERY HOSTNAME)

message(STATUS "----------------------------------------------------------")
message(STATUS "🔍 检测到主机名: ${CURRENT_PC_NAME}")

if(CURRENT_PC_NAME STREQUAL "MSI")
    # 电脑 A: 使用 C 盘路径
    set(RASC_EXE_PATH "C:/Renesas/RA/sc_v2025-12_fsp_v6.3.0/eclipse/rasc.exe")
    message(STATUS "✅ 已匹配 MSI,设置 RASC 路径为 C 盘")

elseif(CURRENT_PC_NAME STREQUAL "PC-20260313EOXE")
    # 电脑 B: 使用 D 盘路径
    set(RASC_EXE_PATH "D:/Renesas/rasc/eclipse/rasc.exe")
    message(STATUS "✅ 已匹配 PC-20260313EOXE,设置 RASC 路径为 D 盘")

else()
    # 防呆逻辑：万一主机名变了或者在第三台电脑上运行
    set(RASC_EXE_PATH "C:/Renesas/RA/sc_v2025-12_fsp_v6.3.0/eclipse/rasc.exe")
    message(WARNING "⚠️ 主机名 [${CURRENT_PC_NAME}] 未记录！已尝试默认 C 盘路径。")
endif()

message(STATUS "🚀 最终 RASC 调用路径: ${RASC_EXE_PATH}")
message(STATUS "----------------------------------------------------------")