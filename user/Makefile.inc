
# OBJ_DIRS += user

USER_LDFLAGS := $(LDFLAGS) -T $(TOOLS_DIR)/user.ld -nostdlib


USER_INC += user \
        user/libs \
        lib

USER_SRC_FILES := user/libs/init_code.S \
        user/libs/stdio.c \
        user/libs/syscall.c \
        user/libs/ulib.c \
        user/libs/umain.c

USER_BINS_SRC_FILES := user/hello.c \
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

$(USER_BINS): $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(USER_OBJ_BINS_FILES)
	@echo + ld $@
	@echo +++++++++ user bins: $(USER_BINS)
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)hello $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/hello.o
	$(V)$(LD) -o $(OBJ_DIR)/user/$(USER_PREFIX)yield $(USER_LDFLAGS) $(USER_OBJ_FILES) $(USER_LIB_OBJ_FILES) $(OBJ_DIR)/user/yield.o

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
