
OBJ_DIRS += boot
BOOT_OBJS := $(OBJ_DIR)/boot/boot.o $(OBJ_DIR)/boot/bootmain.o

$(OBJ_DIR)/boot/%.o: boot/%.c
	echo + cc -Os $<
	mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(KERNEL_CFLAGS) -Os -c -o $@ $<

$(OBJ_DIR)/boot/%.o: boot/%.S
	echo + as $<
	mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(KERNEL_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/boot/sign: $(TOOLS_DIR)/sign.c
	$(CC) -o $@ $<

$(OBJ_DIR)/boot/boot: $(BOOT_OBJS) $(OBJ_DIR)/boot/sign
	echo + ld boot/boot
	# 通过如下的链接方式.data数据段是没有被链接进来的
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7c00 -o $@.out $(BOOT_OBJS)
	$(OBJDUMP) -S $@.out >$@.asm
	$(OBJCOPY) -S -O binary -j .text $@.out $@
	$(OBJ_DIR)/boot/sign $@
