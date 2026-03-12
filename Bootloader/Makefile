# ===============================
# Project Settings
# ===============================
TARGET      := bootloader
BUILD_DIR   := build

# ===============================
# Toolchain Configuration
# ===============================
# 如果没定义，默认使用该路径 (方便你在命令行临时覆盖)
TOOLCHAIN_PREFIX ?= /home/estrella/tools/gcc-arm-none-eabi-10.3-2021.10/bin/arm-none-eabi-

CC      := $(TOOLCHAIN_PREFIX)gcc
AS      := $(TOOLCHAIN_PREFIX)gcc -x assembler-with-cpp
LD      := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
SZ      := $(TOOLCHAIN_PREFIX)size

# ===============================
# MCU / CPU Flags
# ===============================
CPU     := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# 定义共享目录的相对路径 (退回上一级，再进入 Shared)
SHARED_DIR := ../Shared

# ===============================
# Directories
# ===============================
# 包含头文件的路径
INC_DIRS := \
    inc \
    platform/cmsis/core \
    platform/cmsis/device \
    platform/driver/inc \
	$(SHARED_DIR)/inc

# 包含 .c 源文件的路径
SRC_DIRS := \
    src \
    platform/cmsis/device \
	platform/driver/src   \
	driver

STARTUP_DIR := startup
LINKER_DIR  := linker

# ===============================
# File Lists (自动搜索)
# ===============================
# 1. 搜索所有的 .c 文件
C_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))

# 2. 设置启动文件 (注意：这里帮你改成了 GCC 专用的文件)
ASM_SRCS := $(STARTUP_DIR)/startup_stm32f40_41xxx.s

# 3. 生成目标对象文件列表 (.c -> build/xxx.o)
OBJS := $(C_SRCS:%.c=$(BUILD_DIR)/%.o)
OBJS += $(ASM_SRCS:%.s=$(BUILD_DIR)/%.o)

# 4. 生成依赖文件列表 (.d)，用于头文件修改后的自动重编译
DEPS := $(OBJS:.o=.d)

# ===============================
# Compiler Flags
# ===============================
# -MMD -MP: 生成 .d 依赖文件
CFLAGS  := $(CPU) -O0 -g -Wall -ffreestanding -fno-builtin
#告诉编译器，把每一个函数和变量都单独装在一个个小盒子里
CFLAGS += -ffunction-sections -fdata-sections 
#关闭（no-）关于“误导性缩进（misleading-indentation）”的警告（-W）。  
CFLAGS += -Wno-misleading-indentation
#空间极限优化指令
CFLAGS += -Os
CFLAGS  += -DSTM32F40_41xxx
CFLAGS  += -DUSE_STDPERIPH_DRIVER
CFLAGS  += -MMD -MP
CFLAGS  += $(addprefix -I,$(INC_DIRS))

# ===============================
# Linker Flags
# ===============================
LDSCRIPT := $(LINKER_DIR)/STM32F417IGHx_FLASH.ld
#LDFLAGS += -Wl,--gc-sections: 告诉链接器，去检查这些盒子，谁没被调用过，就直接扔进垃圾桶，绝对不打包进最终的 bin 里
LDFLAGS  := $(CPU) -specs=nano.specs -specs=nosys.specs -T$(LDSCRIPT) -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

# ===============================
# Build Rules
# ===============================
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin

# 1. 编译 C 文件
# 说明：@mkdir -p $(dir $@) 是核心，自动创建目录
$(BUILD_DIR)/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	@echo "[CC]  $<"
	@$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(@:.o=.lst) $< -o $@

# 2. 编译汇编文件
$(BUILD_DIR)/%.o: %.s Makefile
	@mkdir -p $(dir $@)
	@echo "[AS]  $<"
	@$(AS) $(CPU) -c $< -o $@

# 3. 链接
$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	@echo "[LD]  $@"
	@$(LD) $(OBJS) $(LDFLAGS) -o $@
	@$(SZ) $@

# 4. 生成 Bin
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	@echo "[HEX] $@"
	@$(OBJCOPY) -O binary $< $@

# Clean
clean:
	@echo "[RM]  $(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)

# 包含依赖文件（如果存在）
-include $(DEPS)

.PHONY: all clean