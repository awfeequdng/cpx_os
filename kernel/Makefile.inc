
OBJ_DIRS += kernel

KERNEL_LDFLAGS := $(LDFLAGS) -T kernel/kernel.ld -nostdlib

KERNEL_SRC_FILES := kernel/entry.S \
		kernel/init.c \
		kernel/entry_pg_dir.c \
		kernel/console.c \
		kernel/printk.c


KERNEL_LIB_SRC_FILES = lib/string.c \
		lib/printfmt.c


# 只替换%.c -> %.o, 没有匹配的保持不变
KERNEL_OBJ_FILES := $(patsubst %.c, $(OBJ_DIR)/%.o, $(KERNEL_SRC_FILES))
# 接着替换%.S -> %.o
KERNEL_OBJ_FILES := $(patsubst %.S, $(OBJ_DIR)/%.o, $(KERNEL_OBJ_FILES))

# 替换lib目录库文件%.c -> %.o
KERNEL_LIB_OBJ_FILES := $(patsubst %.c, $(OBJ_DIR)/%.o, $(KERNEL_LIB_SRC_FILES))


$(OBJ_DIR)/kernel/kernel: $(KERNEL_OBJ_FILES) $(KERNEL_LIB_OBJ_FILES) kernel/kernel.ld
	@echo + ld $@
	$(V)$(LD) -o $@ $(KERNEL_LDFLAGS) $(KERNEL_OBJ_FILES) $(KERNEL_LIB_OBJ_FILES)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(OBJ_DIR)/lib/%.o: lib/%.c
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/kernel/%.o: kernel/%.c
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/kernel/%.o: kernel/%.S
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(KERNEL_CFLAGS) -c -o $@ $<
