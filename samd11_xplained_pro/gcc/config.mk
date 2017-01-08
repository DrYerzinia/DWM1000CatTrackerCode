# Path to top level ASF directory relative to this project directory.
PRJ_PATH = ../../../../..

# Target CPU architecture: cortex-m3, cortex-m4
ARCH = cortex-m0plus

PART = samd11d14am

# Application target name. Given with suffix .a for library and .elf for a
# standalone application.
TARGET_FLASH = DECA_Ancor_flash.elf
TARGET_SRAM = DECA_Ancor_sram.elf

# List of C source files.
CSRCS = \
       common/utils/interrupt/interrupt_sam_nvic.c        \
       common2/services/delay/sam0/systick_counter.c      \
       sam0/applications/DECA/main.c                      \
       sam0/applications/DECA/console.c                   \
       sam0/applications/DECA/deca_device.c               \
       sam0/applications/DECA/deca_mutex.c                \
       sam0/applications/DECA/deca_params_init.c          \
       sam0/applications/DECA/deca_range_tables.c         \
       sam0/applications/DECA/deca_sleep.c                \
       sam0/applications/DECA/deca_spi.c                  \
       sam0/boards/samd11_xplained_pro/board_init.c       \
       sam0/drivers/events/events_hooks.c                 \
       sam0/drivers/events/events_sam_d_r/events.c        \
       sam0/drivers/nvm/nvm.c                             \
       sam0/drivers/port/port.c                           \
       sam0/drivers/extint/extint_callback.c              \
       sam0/drivers/extint/extint_sam_d_r/extint.c        \
       sam0/drivers/rtc/rtc_sam_d_r/rtc_count.c           \
       sam0/drivers/rtc/rtc_sam_d_r/rtc_count_interrupt.c \
       sam0/drivers/sercom/sercom.c                       \
       sam0/drivers/sercom/sercom_interrupt.c             \
       sam0/drivers/sercom/spi/spi.c                      \
       sam0/drivers/sercom/usart/usart.c                  \
       sam0/drivers/sercom/usart/usart_interrupt.c        \
       sam0/drivers/system/clock/clock_samd09_d10_d11/clock.c     \
       sam0/drivers/system/clock/clock_samd09_d10_d11/gclk.c      \
       sam0/drivers/system/interrupt/system_interrupt.c   \
       sam0/drivers/system/pinmux/pinmux.c                \
       sam0/drivers/system/system.c                       \
       sam0/drivers/tc/tc_sam_d_r/tc.c                    \
       sam0/services/eeprom/emulator/main_array/eeprom.c  \
       sam0/utils/cmsis/samd11/source/gcc/startup_samd11.c \
       sam0/utils/cmsis/samd11/source/system_samd11.c     \
       sam0/utils/stdio/read.c                            \
       sam0/utils/stdio/write.c                           \
       sam0/utils/syscalls/gcc/syscalls.c


# List of assembler source files.
ASSRCS = 

# List of include paths.
INC_PATH = \
       common/boards                                      \
       common/utils                                       \
       common/services/serial                             \
       common2/services/delay                             \
       common2/services/delay/sam0                        \
       sam0/applications/DECA                             \
       sam0/applications/DECA/samd11_xplained_pro         \
       sam0/boards                                        \
       sam0/boards/samd11_xplained_pro                    \
       sam0/drivers/events                                \
       sam0/drivers/events/events_sam_d_r                 \
       sam0/drivers/nvm                                   \
       sam0/drivers/port                                  \
       sam0/drivers/rtc                                   \
       sam0/drivers/rtc/rtc_sam_d_r                       \
       sam0/drivers/sercom                                \
       sam0/drivers/extint                                \
       sam0/drivers/sercom/spi                            \
       sam0/drivers/sercom/usart                          \
       sam0/drivers/system                                \
       sam0/drivers/system/clock                          \
       sam0/drivers/system/clock/clock_samd09_d10_d11     \
       sam0/drivers/system/interrupt                      \
       sam0/drivers/system/interrupt/system_interrupt_samd10_d11 \
       sam0/drivers/system/pinmux                         \
       sam0/drivers/system/power                          \
       sam0/drivers/system/power/power_sam_d_r            \
       sam0/drivers/system/reset                          \
       sam0/drivers/system/reset/reset_sam_d_r            \
       sam0/drivers/tc                                    \
       sam0/drivers/tc/tc_sam_d_r                         \
       sam0/services/eeprom/emulator/main_array          \
       sam0/utils                                         \
       sam0/utils/cmsis/samd11/include                    \
       sam0/utils/cmsis/samd11/source                     \
       sam0/utils/header_files                            \
       sam0/utils/preprocessor                            \
       sam0/utils/stdio/stdio_serial                      \
       thirdparty/CMSIS/Include                           \
       thirdparty/CMSIS/Lib/GCC \
       sam0/applications/DECA/samd11_xplained_pro/gcc

# Additional search paths for libraries.
LIB_PATH =  \
       thirdparty/CMSIS/Lib/GCC                          

# List of libraries to use during linking.
LIBS =  \
       arm_cortexM0l_math                                

# Path relative to top level directory pointing to a linker script.
LINKER_SCRIPT_FLASH = sam0/utils/linker_scripts/samd11/gcc/samd11d14am_flash.ld
LINKER_SCRIPT_SRAM  = sam0/utils/linker_scripts/samd11/gcc/samd11d14am_sram.ld

# Path relative to top level directory pointing to a linker script.
DEBUG_SCRIPT_FLASH = sam0/boards/samd11_xplained_pro/debug_scripts/gcc/samd11_xplained_pro_flash.gdb
DEBUG_SCRIPT_SRAM  = sam0/boards/samd11_xplained_pro/debug_scripts/gcc/samd11_xplained_pro_sram.gdb

# Project type parameter: all, sram or flash
PROJECT_TYPE        = flash

# Additional options for debugging. By default the common Makefile.in will
# add -g3.
DBGFLAGS = 

# Application optimization used during compilation and linking:
# -O0, -O1, -O2, -O3 or -Os
OPTIMIZATION = -Os

# Extra flags to use when archiving.
ARFLAGS = 

# Extra flags to use when assembling.
ASFLAGS = 

# Extra flags to use when compiling.
CFLAGS = 

# Extra flags to use when preprocessing.
#
# Preprocessor symbol definitions
#   To add a definition use the format "-D name[=definition]".
#   To cancel a definition use the format "-U name".
#
# The most relevant symbols to define for the preprocessor are:
#   BOARD      Target board in use, see boards/board.h for a list.
#   EXT_BOARD  Optional extension board in use, see boards/board.h for a list.
CPPFLAGS = \
       -D ARM_MATH_CM0PLUS=true                           \
       -D BOARD=SAMD11_XPLAINED_PRO                       \
       -D SYSTICK_MODE                                    \
       -D RTC_COUNT_ASYNC=true                            \
       -D USART_CALLBACK_MODE=true                        \
       -D SPI_CALLBACK_MODE=false                         \
       -D EXTINT_CALLBACK_MODE=true                       \
       -D EVENTS_INTERRUPT_HOOKS_MODE=true                \
       -D TC_ASYNC=false                                  \
       -D __SAMD11D14AM__				  \
       -D TINY_ROM

# Extra flags to use when linking
LDFLAGS = \

# Pre- and post-build commands
PREBUILD_CMD = 
POSTBUILD_CMD = 
