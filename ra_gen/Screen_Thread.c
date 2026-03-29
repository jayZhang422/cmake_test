/* generated thread source file - do not edit */
#include "Screen_Thread.h"

TX_THREAD Screen_Thread;
                void Screen_Thread_create(void);
                static void Screen_Thread_func(ULONG thread_input);
                static uint8_t Screen_Thread_stack[10240] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.Screen_Thread") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                void tx_startup_err_callback(void * p_instance, void * p_data);
                void tx_startup_common_init(void);
extern bool         g_fsp_common_initialized;
                extern uint32_t     g_fsp_common_thread_count;
                extern TX_SEMAPHORE g_fsp_common_initialized_semaphore;

                void Screen_Thread_create (void)
                {
                    /* Increment count so we will know the number of ISDE created threads. */
                    g_fsp_common_thread_count++;

                    /* Initialize each kernel object. */
                    

                    UINT err;
                    err = tx_thread_create(
                        &Screen_Thread,
                        (CHAR *)"Screen_Thread",
                        Screen_Thread_func,
                        (ULONG) NULL,
                        &Screen_Thread_stack,
                        10240,
                        1,
                        1,
                        1,
                        TX_AUTO_START
                    );
                    if (TX_SUCCESS != err) {
                        tx_startup_err_callback(&Screen_Thread, 0);
                    }
                }

                static void Screen_Thread_func (ULONG thread_input) {
                    /* Not currently using thread_input. */
                    FSP_PARAMETER_NOT_USED(thread_input);

                    /* Initialize common components */
                    tx_startup_common_init();

                    /* Initialize each module instance. */
                    

                    /* Enter user code for this thread. */
                    Screen_Thread_entry();
                }
