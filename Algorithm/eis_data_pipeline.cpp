#include "eis_data_pipeline.hpp"

namespace EIS {

// 在 .cpp 文件中真正分配内存，这样无论头文件被包含多少次，内存只分配一次
TX_EVENT_FLAGS_GROUP DataPipeline::_dma_events;
bool                 DataPipeline::_current_buffer_is_ping = true;

// 强制 4 字节对齐的实际内存分配
alignas(4) uint16_t  DataPipeline::_ping_buffer[MAX_DMA_BUFFER_SIZE];
alignas(4) uint16_t  DataPipeline::_pong_buffer[MAX_DMA_BUFFER_SIZE];

} // namespace EIS