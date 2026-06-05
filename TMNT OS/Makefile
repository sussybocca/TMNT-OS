# TMNT OS Makefile

ASM=nasm
CC=gcc
LD=ld

CFLAGS=-m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector -Wall -Wextra -I.
LDFLAGS=-m elf_i386 -T linker.ld

SOURCES=kernel/kernel.c \
        drivers/vga/vga.c \
        drivers/keyboard/keyboard.c \
        fs/tmnt_fs.c \
        system/memory.c \
        system/string.c \
        system/config.c \
        system/image_injector.c \
        shell/shell.c

OBJECTS=$(SOURCES:.c=.o)

all: tmnt-os.iso

boot/boot.bin: boot/boot.asm
	$(ASM) -f bin $< -o $@

boot/gdt.o: boot/gdt.asm
	$(ASM) -f elf32 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: boot/gdt.o $(OBJECTS)
	$(LD) $(LDFLAGS) $^ -o $@

tmnt-os.iso: boot/boot.bin kernel.bin
	mkdir -p iso/boot/grub
	cp boot/boot.bin iso/boot/
	cp kernel.bin iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o tmnt-os.iso iso/

run: tmnt-os.iso
	qemu-system-x86_64 -cdrom tmnt-os.iso

clean:
	rm -f *.o *.bin tmnt-os.iso
	rm -f boot/*.o boot/*.bin
	rm -f kernel/*.o
	rm -f drivers/**/*.o
	rm -f fs/*.o
	rm -f system/*.o
	rm -f shell/*.o
	rm -rf iso

.PHONY: all run clean
