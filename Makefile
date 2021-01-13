
OBJ_DIR := obj
TOOLS_DIR := tools
TOP = .
# OBJ_DIRS :=
KERNEL_INC = $(TOP) \
			 kernel/ \
			 lib/

ifndef COMPILE_PREFIX
COMPILE_PREFIX := $(shell if objdump -i 2>&1 | grep 'elf32-i386' > /dev/null 2>&1; \
	then echo ''; \
	else ehco "***" 1>&2; \
	echo "*** error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	ehco "***" 1>&2; exit 1; fi)
endif

ifndef QEMU
QEMU := $(shell if which qemu > /dev/null; \
	then echo qemu; exit; \
	elif which qemu-system-i386 > /dev/null; \
	then echo qemu-system-i386; exit; \
	elif which qemu-system-x86_64 > /dev/null; \
	then echo qemu-system-x86_64; exit; \
	fi; \
	echo "***" 1>&2; \
	echo "*** Error: Couldn't find a working QEMU executable." 1>&2; \
	echo "*** Is the directory containing the qemu binary in your PATH" 1>&2; \
	echo "*** or have you tried setting the QEMU variable in Makefile?" 1>&2; \
        echo "***" 1>&2; exit 1)
endif

CC = $(COMPILE_PREFIX)gcc
#AS = $(COMPILE_PREFIX)gas
LD = $(COMPILE_PREFIX)ld
OBJCOPY = $(COMPILE_PREFIX)objcopy
OBJDUMP = $(COMPILE_PREFIX)objdump
NM	= $(COMPILE_PREFIX)nm

# 编译选项详情见：https://zhuanlan.zhihu.com/p/316007378
CFLAGS := $(CFLAGS) -fno-builtin -m32 -O0
CFLAGS += -static -std=gnu99
CFLAGS += -fno-omit-frame-pointer -Wall -MD 
# 不需要位置无关代码
# CFLAGS += -fno-pie -fno-pic
CFLAGS += -fno-pie 
CFLAGS += -g -ggdb -gstabs -gstabs+ 
# 加上-fno-stack-protector编译选项，不然程序会报'lib/printfmt.c:247: undefined reference to `__stack_chk_fail_local'错误
# 如果没有该选项，编译器会判断vsnprintf这个函数可能会出现缓冲区溢出的风险，因此会调用编译器builtin函数__stack_chk_fail_local，
# 但我们添加了-fno-builtin选项，因此会报找不到__stack_chk_fail_local这个函数的错误。
# 正确的方法就是我们禁用堆栈保护功能
# CFLAGS += -fno-stack-protector
# Add -fno-stack-protector if the option exists.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)


# Common linker flags
LDFLAGS := -m elf_i386

# 定义CPX_OS_KERNEL宏，表示当前是内核态代码，通过这个宏来区分内核态和用户态代码
KERNEL_CFLAGS := $(CFLAGS) -DCPX_OS_KERNEL

ASFLAGS = -m32


# 用户态代码编译选项
USER_PREFIX := 
USER_CFLAGS :=
USER_BINS :=

IMAGES = $(OBJ_DIR)/kernel/cpx_os
IMAGES_TMP = $(IMAGES)~
KERNEL = $(OBJ_DIR)/kernel/kernel
BOOT = $(OBJ_DIR)/boot/boot
SWAPIMG = $(OBJ_DIR)/swap.img

all: build
# all: $(IMAGES) $(SWAPIMG)

$(IMAGES): $(KERNEL) $(BOOT)
	@echo + mk $@
	$(V)dd if=/dev/zero of=$(IMAGES_TMP) count=10000 2>/dev/null
	$(V)dd if=$(BOOT) of=$(IMAGES_TMP) conv=notrunc 2>/dev/null
	$(V)dd if=$(KERNEL) of=$(IMAGES_TMP) seek=1 conv=notrunc 2>/dev/null
	$(V)mv $(IMAGES_TMP)  $(IMAGES)

$(SWAPIMG):
	$(V)dd if=/dev/zero of=$@ bs=1M count=128

include user/Makefile.inc
include boot/Makefile.inc
include kernel/Makefile.inc

build: $(USER_BINS) $(IMAGES) $(SWAPIMG)

GDB_PORT = $(shell expr `id -u` % 5000 + 25000)

# QEMUOPTS = -S -s -hda ./$(IMAGES) -monitor stdio 
QEMUOPTS = -m 64m -drive file=$(IMAGES),index=0,media=disk,format=raw -serial mon:stdio -gdb tcp::$(GDB_PORT)
QEMUOPTS += -drive file=$(SWAPIMG),index=1,media=disk,format=raw,cache=writeback
# QEMUOPTS = -drive file=$(IMAGES),index=0,media=disk,format=raw -gdb tcp::$(GDB_PORT)

.gdbinit: .gdbinit.tmp
	sed "s/localhost:1234/localhost:$(GDB_PORT)/" < $< > $@

gdb:
	gdb -n -x .gdbinit


pre-qemu: .gdbinit



qemu: all pre-qemu
	$(QEMU)  $(QEMUOPTS)

qemu-nox: all pre-qemu
	@echo "***"
	@echo "*** Use Ctrl-a x to exit qemu"
	@echo "***"
	$(QEMU) -nographic  $(QEMUOPTS)

qemu-gdb: all pre-qemu
	@echo "***"
	@echo "*** Now run 'make gdb'." 1>&2
	@echo "***"
	# 通过qemu的 -S -s参数让qemu启动内核后不执行，然后通过gdb进行调试
	$(QEMU) $(QEMUOPTS) -S

qemu-nox-gdb: all pre-qemu
	@echo "***"
	@echo "*** Now run 'make gdb'." 1>&2
	@echo "***"
	# 通过qemu的 -S -s参数让qemu启动内核后不执行，然后通过gdb进行调试
	$(QEMU) -nographic $(QEMUOPTS) -S

TERMINAL := gnome-terminal
debug: all pre-qemu
	$(V)$(QEMU) -S -s $(QEMUOPTS) &
	$(V)sleep 2
	$(V)$(TERMINAL) -e "gdb -n -x .gdbinit"

print-qemu:
	@echo $(QEMU)

print-gdbport:
	@echo $(GDB_PORT)

clean:
	rm obj/ -rf

