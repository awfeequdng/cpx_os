
OBJ_DIR := obj
TOOLS_DIR := tools
TOP = .
OBJ_DIRS :=

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
CFLAGS := $(CFLAGS) -I$(TOP) -fno-builtin -m32 -O1
CFLAGS += -static -std=gnu99
CFLAGS += -fno-omit-frame-pointer -Wall -MD 
CFLAGS += -g -ggdb -gstabs 

# Common linker flags
LDFLAGS := -m elf_i386

# 定义CPX_OS_KERNEL宏，表示当前是内核态代码，通过这个宏来区分内核态和用户态代码
KERNEL_CFLAGS := $(CFLAGS) -DCPX_OS_KERNEL

ASFLAGS = -m32


IMAGES = $(OBJ_DIR)/kernel/cpx_os
IMAGES_TMP = $(IMAGES)~
KERNEL = $(OBJ_DIR)/kernel/kernel
BOOT = $(OBJ_DIR)/boot/boot

all: $(IMAGES)

$(IMAGES): $(KERNEL) $(BOOT)
	@echo + mk $@
	$(V)dd if=/dev/zero of=$(IMAGES_TMP) count=10000 2>/dev/null
	$(V)dd if=$(BOOT) of=$(IMAGES_TMP) conv=notrunc 2>/dev/null
	$(V)dd if=$(KERNEL) of=$(IMAGES_TMP) seek=1 conv=notrunc 2>/dev/null
	$(V)mv $(IMAGES_TMP)  $(IMAGES)


include boot/Makefile.inc
include kernel/Makefile.inc

GDB_PORT = $(shell expr `id -u` % 5000 + 25000)

# QEMUOPTS = -S -s -hda ./$(IMAGES) -monitor stdio 
QEMUOPTS = -drive file=$(IMAGES),index=0,media=disk,format=raw -serial mon:stdio -gdb tcp::$(GDB_PORT)
#QEMUOPTS = -drive file=$(IMAGES),index=0,media=disk,format=raw -gdb tcp::$(GDB_PORT)

.gdbinit: .gdbinit.tmp
	sed "s/localhost:1234/localhost:$(GDB_PORT)/" < $< > $@

gdb:
	gdb -n -x .gdbinit


pre-qemu: .gdbinit



qemu: $(IMAGES) pre-qemu
	$(QEMU)  $(QEMUOPTS)

qemu-nox: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Use Ctrl-a x to exit qemu"
	@echo "***"
	$(QEMU) -nographic $(QEMUOPTS)

qemu-gdb: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Now run 'make gdb'." 1>&2
	@echo "***"
	# 通过qemu的 -S -s参数让qemu启动内核后不执行，然后通过gdb进行调试
	$(QEMU) $(QEMUOPTS) -S

qemu-nox-gdb: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Now run 'make gdb'." 1>&2
	@echo "***"
	# 通过qemu的 -S -s参数让qemu启动内核后不执行，然后通过gdb进行调试
	$(QEMU) -nographic $(QEMUOPTS) -S


print-qemu:
	@echo $(QEMU)

print-gdbport:
	@echo $(GDB_PORT)

clean:
	rm obj/ -rf

