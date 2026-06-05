#!/bin/bash

echo "🐢 Building TMNT OS from scratch 🐢"
echo "===================================="

# Install dependencies
if ! command -v nasm &> /dev/null; then
    echo "Installing NASM..."
    sudo apt-get install -y nasm
fi

if ! command -v grub-mkrescue &> /dev/null; then
    echo "Installing GRUB tools..."
    sudo apt-get install -y grub-pc-bin grub-efi-amd64-bin xorriso mtools
fi

# Clean
echo "Cleaning..."
rm -f *.o *.bin tmnt-os.iso
rm -f boot/*.o boot/*.bin
rm -f kernel/*.o
rm -f drivers/vga/*.o
rm -f drivers/keyboard/*.o
rm -f fs/*.o
rm -f system/*.o
rm -f shell/*.o
rm -f apps/downloader/*.o
rm -rf iso

# Compile bootloader
echo "Compiling bootloader..."
nasm -f bin boot/boot.asm -o boot/boot.bin

# Compile kernel and drivers
echo "Compiling kernel..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -Wall -Wextra -I. \
    -c kernel/kernel.c -o kernel/kernel.o

echo "Compiling VGA driver..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -Wall -Wextra -I. \
    -c drivers/vga/vga.c -o drivers/vga/vga.o

echo "Compiling keyboard driver..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -Wall -Wextra -I. \
    -c drivers/keyboard/keyboard.c -o drivers/keyboard/keyboard.o

echo "Compiling filesystem..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -Wall -Wextra -I. \
    -c fs/tmnt_fs.c -o fs/tmnt_fs.o

echo "Compiling memory manager..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -Wall -Wextra -I. \
    -c system/memory.c -o system/memory.o

echo "Compiling string functions..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -Wall -Wextra -I. \
    -c system/string.c -o system/string.o

echo "Compiling config system..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -Wall -Wextra -I. \
    -c system/config.c -o system/config.o

echo "Compiling image injector..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -Wall -Wextra -I. \
    -c system/image_injector.c -o system/image_injector.o

echo "Compiling shell..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -Wall -Wextra -I. \
    -c shell/shell.c -o shell/shell.o

echo "Compiling downloader app..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -Wall -Wextra -I. \
    -c apps/downloader/downloader.c -o apps/downloader/downloader.o

# Link kernel
echo "Linking kernel..."
ld -m elf_i386 -T linker.ld \
    kernel/kernel.o \
    drivers/vga/vga.o \
    drivers/keyboard/keyboard.o \
    fs/tmnt_fs.o \
    system/memory.o \
    system/string.o \
    system/config.o \
    system/image_injector.o \
    shell/shell.o \
    apps/downloader/downloader.o \
    -o kernel.bin

# Create ISO structure
echo "Creating ISO..."
mkdir -p iso/boot/grub
cp boot/boot.bin iso/boot/
cp kernel.bin iso/boot/
cp grub.cfg iso/boot/grub/

# Build ISO
grub-mkrescue -o tmnt-os.iso iso/ 2>/dev/null

if [ -f "tmnt-os.iso" ]; then
    echo ""
    echo "✅ TMNT OS built successfully!"
    echo "📀 ISO file: tmnt-os.iso ($(du -h tmnt-os.iso | cut -f1))"
    echo ""
    echo "🐢 Included apps:"
    echo "   - Downloader (Offline App Store)"
    echo "   - Settings"
    echo "   - Shell"
    echo ""
    echo "To run: qemu-system-x86_64 -cdrom tmnt-os.iso -m 256M"
    echo "To burn: sudo dd if=tmnt-os.iso of=/dev/sdX bs=4M status=progress"
    echo ""
    echo "🍕 Cowabunga! Turtle Power! 🍕"
else
    echo "❌ Build failed!"
fi