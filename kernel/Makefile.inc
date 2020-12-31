
OBJ_DIRS += kernel

KERNEL_LDFLAGS := $(LDFLAGS) -T $(TOOLS_DIR)/kernel.ld -nostdlib


KERNEL_INC += kernel/driver \
			  kernel/debug \
			  kernel/trap \
			  kernel/init \
			  kernel/mm \
			  kernel/lib \
			  kernel/sync

KERNEL_SRC_FILES := kernel/init/entry.S \
		kernel/trap/trap.c \
		kernel/trap/trap_entry.S \
		kernel/init/init.c \
		kernel/init/entry_pg_dir.c \
		kernel/driver/console.c \
		kernel/printk.c \
		kernel/debug/kmonitor.c \
		kernel/debug/kdebug.c \
		kernel/driver/pic_irq.c \
		kernel/lib/stdio.c \
		kernel/debug/panic.c \
		kernel/driver/clock.c \
		kernel/mm/pmm.c \
		kernel/mm/default_pmm.c \
		kernel/mm/bestfit_pmm.c \
		kernel/mm/buddy_pmm.c \
		kernel/mm/slab.c \
		kernel/mm/vmm.c \
		kernel/lib/rbtree.c


KERNEL_LIB_SRC_FILES = lib/string.c \
		lib/printfmt.c \
		lib/readline.c \
		lib/rand.c


# 只替换%.c -> %.o, 没有匹配的保持不变
KERNEL_OBJ_FILES := $(patsubst %.c, $(OBJ_DIR)/%.o, $(KERNEL_SRC_FILES))
# 接着替换%.S -> %.o
KERNEL_OBJ_FILES := $(patsubst %.S, $(OBJ_DIR)/%.o, $(KERNEL_OBJ_FILES))

# 替换lib目录库文件%.c -> %.o
KERNEL_LIB_OBJ_FILES := $(patsubst %.c, $(OBJ_DIR)/%.o, $(KERNEL_LIB_SRC_FILES))

# 在KERNEL_INC的每个元素前面添加-I
KERNEL_CFLAGS += $(addprefix -I,$(KERNEL_INC))

$(OBJ_DIR)/vector.o: $(TOOLS_DIR)/vector.c
	@echo + cc $<
	@mkdir -p $(@D)
	gcc -o $(OBJ_DIR)/vector $<
	$(OBJ_DIR)/vector >> $(OBJ_DIR)/vector.S
	$(V)$(CC) -nostdinc $(KERNEL_CFLAGS) -c -o $@ $(OBJ_DIR)/vector.S 
	
	

$(OBJ_DIR)/kernel/kernel: $(KERNEL_OBJ_FILES) $(KERNEL_LIB_OBJ_FILES) $(OBJ_DIR)/vector.o
	@echo + ld $@
	$(V)$(LD) -o $@ $(KERNEL_LDFLAGS) $^ 
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
