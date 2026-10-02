set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# Some default GCC settings. Prefer a toolchain on PATH, but also support the
# copy bundled with STM32CubeIDE on Windows.
set(TOOLCHAIN_PREFIX                arm-none-eabi-)

set(_ARM_GCC_HINTS)
if(WIN32)
    file(GLOB _STM32CUBE_GCC_DIRS
        "C:/ST/STM32CubeIDE_*/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin"
        "C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin"
    )
    list(SORT _STM32CUBE_GCC_DIRS ORDER DESCENDING)
    list(APPEND _ARM_GCC_HINTS ${_STM32CUBE_GCC_DIRS})
endif()

find_program(CMAKE_C_COMPILER NAMES ${TOOLCHAIN_PREFIX}gcc HINTS ${_ARM_GCC_HINTS} REQUIRED)
get_filename_component(_ARM_GCC_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
set(CMAKE_ASM_COMPILER              ${CMAKE_C_COMPILER})
find_program(CMAKE_CXX_COMPILER NAMES ${TOOLCHAIN_PREFIX}g++ HINTS "${_ARM_GCC_BIN_DIR}" REQUIRED)
find_program(CMAKE_LINKER NAMES ${TOOLCHAIN_PREFIX}g++ HINTS "${_ARM_GCC_BIN_DIR}" REQUIRED)
find_program(CMAKE_OBJCOPY NAMES ${TOOLCHAIN_PREFIX}objcopy HINTS "${_ARM_GCC_BIN_DIR}" REQUIRED)
find_program(CMAKE_SIZE NAMES ${TOOLCHAIN_PREFIX}size HINTS "${_ARM_GCC_BIN_DIR}" REQUIRED)

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections -fstack-usage")

# The cyclomatic-complexity parameter must be defined for the Cyclomatic complexity feature in STM32CubeIDE to work.
# However, most GCC toolchains do not support this option, which causes a compilation error; for this reason, the feature is disabled by default.
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcyclomatic-complexity")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32L496xx_FLASH.ld\"")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(TOOLCHAIN_LINK_LIBRARIES "m")
