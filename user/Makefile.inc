
# OBJ_DIRS += user

USER_LDFLAGS := $(LDFLAGS) -T $(TOOLS_DIR)/user.ld -nostdlib


USER_INC += user \
        user/libs \
        lib

USER_SRC_FILES := user/libs/init_code.S \
        user/libs/stdio.c \
        user/libs/syscall.c \
        user/libs/ulib.c \
        user/libs/umain.c \
		user/libs/panic.c \
		user/libs/malloc.c \
		user/libs/thread.c \
		user/libs/clone.S

USER_BINS_SRC_FILES := \
		user/matrix.c \
		user/thread_fork.c \
		user/skiplist_test.c \
		user/thread_test.c \
		user/shmem_test.c \
		user/mmap_test.c \
		user/swap_test.c \
		user/cow_test.c \
		user/sleep.c \
		user/bad_arg.c \
		user/bad_brk_test.c \
		user/fork_tree.c \
		user/fork_test.c \
		user/exit.c \
		user/test_bss.c \
		user/hello.c \
        user/yield.c 
		

USER_LIB_SRC_FILES = lib/string.c \
		lib/printfmt.c \
		lib/rand.c \
		lib/hash.c


USER_OBJ_BINS_FILES := $(patsubst %.c, $(OBJ_DIR)/%.o, $(USER_BINS_SRC_FILES))

USER_BINS_BASENAME := $(basename $(USER_OBJ_BINS_FILES))
USER_BINS += $(addprefix $(OBJ_DIR)/user/$(USER_PREFIX),$(notdir $(USER_BINS_BASENAME)))

# 只替换%.c -> %.o, 没有匹配的保持不变
USER_OBJ_FILES := $(patsubst %.c, $(OBJ_DIR)/%.o, $(USER_SRC_FILES))
# 接着替换%.S -> %.o
USER_OBJ_FILES := $(patsubst %.S, $(OBJ_DIR)/%.o, $(USER_OBJ_FILES))

# 替换lib目录库文件%.c -> %.o
USER_LIB_OBJ_FILES := $(patsubst %.c, $(OBJ_DIR)/%.o, $(USER_LIB_SRC_FILES))

# 在KERNEL_INC的每个元素前面添加-I
USER_CFLAGS += $(addprefix -I,$(USER_INC))
USER_CFLAGS += $(CFLAGS)

$(USER_OBJ_BINS_FILES): $(USER_BINS_SRC_FILES)
	$(V)$(CC) -nostdinc $(USER_CFLAGS) -c -o $@ $<

$(USER_BINS): $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(USER_OBJ_BINS_FILES)
	@echo + ld $@
	@echo +++++++++ user bins: $(USER_BINS)
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)hello $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/hello.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)yield $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/yield.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)test_bss $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/test_bss.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)exit $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/exit.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)fork_test $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/fork_test.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)fork_tree $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/fork_tree.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)bad_brk_test $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/bad_brk_test.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)bad_arg $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/bad_arg.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)sleep $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/sleep.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)cow_test $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/cow_test.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)swap_test $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/swap_test.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)mmap_test $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/mmap_test.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)shmem_test $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/shmem_test.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)thread_test $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/thread_test.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)skiplist_test $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/skiplist_test.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)thread_fork $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/thread_fork.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)matrix $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/matrix.o

# $(OBJ_DIR)/lib/%.o:

$(OBJ_DIR)/lib/%.o: lib/%.c
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(USER_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/user/%.o: user/%.c
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(USER_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/user/%.o: user/%.S
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(USER_CFLAGS) -c -o $@ $<
