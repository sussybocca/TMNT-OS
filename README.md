# 🐢 TMNT OS - Teenage Mutant Ninja Turtles Operating System

![TMNT OS](https://img.shields.io/badge/version-1.0-green?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT%20%2B%20Attribution-brightgreen?style=for-the-badge)
![Platform](https://img.shields.io/badge/platform-x86__32-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/status-active%20development-orange?style=for-the-badge)

**COWABUNGA!** A fully custom 32-bit operating system built from absolute scratch, featuring a Teenage Mutant Ninja Turtles retro-modern theme. Complete with GUI desktop, terminal, filesystem, Sound Blaster 16 audio, AI assistant, and more - all running in a pizza-powered sewer lair!

---

## 🚀 Features

### Core System
- **32-bit Protected Mode Kernel** - Custom bootloader, GDT, IDT, memory management
- **Multiboot Compliant** - Boots via GRUB on real hardware or QEMU
- **VGA Text Mode** - 80x25 text display with custom color themes
- **Framebuffer Graphics** - High-resolution GUI with VBE 2.0 support
- **PS/2 Mouse & Keyboard** - Full input support with custom drivers
- **PIT Timer** - Programmable interrupt timer for system timing

### Graphical User Interface
- **TMNT-Themed Desktop** - Dark green sewer aesthetic with turtle cursor
- **Draggable Windows** - Title bars with minimize, maximize, close buttons
- **Desktop Icons** - Clickable app launchers with automatic discovery
- **Custom Turtle Cursor** - 16x16 animated turtle shell pointer
- **8x16 Bitmap Font** - Full ASCII 32-127 character set
- **TMNT-Styled Buttons** - Green gradient buttons with glowing borders

### Applications
- **📁 File Manager** - Browse, open, and manage files
- **📝 Text Editor** - Create and edit text files with multiline support
- **🎨 Paint Studio** - Pixel art drawing application
- **🏃 Runner Game** - Built-in game
- **🎵 Sound Studio** - Music player with playlist management
- **🛠️ TAM App Creator** - Build apps using simple TAM markup language
- **🤖 TMNT Bot AI** - Learning AI assistant that watches everything you do
- **💻 Terminal** - Full command-line interface with TM commands
- **🔧 TM Language** - Custom scripting language interpreter

### File System
- **TMNT FS** - Custom inode-based filesystem
- **File Operations** - Create, read, write, delete, list directories
- **Encryption** - XOR file encryption support
- **Password Protection** - Lock/unlock files with password hashing
- **Directory Structure** - /system/, /apps/, /user/, /user/music/

### Audio
- **Sound Blaster 16** - Real hardware audio support (port 0x220)
- **DMA Transfer** - Direct memory access for smooth playback
- **8-bit PCM** - 8000Hz mono unsigned 8-bit audio
- **Raw Audio Files** - Play .raw music files from /user/music/
- **PC Speaker** - Fallback beeper for basic sounds

### AI Assistant
- **TMNT Bot** - Standalone GUI chat application
- **Learning Service** - Background driver that watches all user input/output
- **Pattern Recognition** - Learns command patterns and responses
- **Knowledge Files** - Loadable .kn knowledge base files
- **App Generation** - Can create new app boilerplate code
- **Persistent Memory** - Saves learned patterns to /system/ai/learned.kn

### Customization
- **10 Background Styles** - Bricks, Sewer, Shell, Manhole, Pizza, Ninja, Radioactive, City, Lair
- **5 Color Themes** - Red, Blue, Purple, Orange, Green
- **Shell Colors** - Custom RGB background and foreground colors
- **Boot Modes** - Terminal mode, Safe mode, Recovery mode

---

## 📦 Project Structure

    TMNT OS/
    ├── boot/                    # Bootloader (NASM assembly)
    │   └── boot.asm
    ├── kernel/                  # Kernel core
    │   └── kernel.c
    ├── drivers/                 # Hardware drivers
    │   ├── vga/                 # VGA text mode driver
    │   ├── sb16.c/.h            # Sound Blaster 16 audio
    │   ├── pit.c/.h             # Programmable Interval Timer
    │   └── AI/Background/Services/  # AI background service
    ├── external/                # System UI layer
    │   ├── gui.c/.h             # GUI framework
    │   ├── input.c/.h           # Keyboard/mouse input
    │   └── terminal.c/.h        # Terminal emulator
    ├── system/                  # System libraries
    │   ├── string.c/.h          # String manipulation
    │   ├── memory.c/.h          # Memory management
    │   └── types.h              # Type definitions
    ├── fs/                      # File system
    │   └── tmnt_fs.c/.h         # Custom filesystem
    ├── apps/                    # Applications
    │   ├── file_manager/        # File browser
    │   ├── text_editor/         # Text editor
    │   ├── paint_studio/        # Drawing app
    │   ├── runner/              # Game
    │   ├── sound_studio/        # Music player
    │   ├── tmnt_bot/            # AI assistant
    │   ├── tam_parser/          # TAM app builder
    │   ├── tm_lang/             # TM scripting language
    │   ├── tmnt_maker/          # App maker
    │   ├── turtlenotes/         # Note taking app
    │   └── minipack/            # Mini app pack
    ├── system/ai/knowledge/     # AI knowledge files (.kn)
    ├── iso/                     # ISO build directory
    ├── linker.ld                # Linker script
    ├── Makefile                 # Build system
    └── LICENSE                  # License file

---

## 🛠️ Build Requirements

### Linux (Ubuntu/Debian)
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        nasm \
        gcc-multilib \
        binutils \
        xorriso \
        grub-pc-bin \
        grub-common \
        qemu-system-x86

### Linux (Arch/Manjaro)
    sudo pacman -S \
        base-devel \
        nasm \
        gcc-multilib \
        binutils \
        xorriso \
        grub \
        qemu

### Linux (Fedora)
    sudo dnf install \
        gcc \
        nasm \
        glibc-devel.i686 \
        binutils \
        xorriso \
        grub2 \
        qemu-system-x86

### macOS (via Homebrew)
    brew install \
        nasm \
        xorriso \
        qemu \
        i686-elf-binutils \
        i686-elf-gcc

### Required Tools
| Tool | Purpose | Download |
|------|---------|----------|
| **NASM** | Assembly compiler | https://www.nasm.us/ |
| **GCC (32-bit)** | C compiler | https://gcc.gnu.org/ |
| **GNU Make** | Build system | https://www.gnu.org/software/make/ |
| **GRUB 2** | Bootloader | https://www.gnu.org/software/grub/ |
| **Xorriso** | ISO creation | https://www.gnu.org/software/xorriso/ |
| **QEMU** | Emulator for testing | https://www.qemu.org/ |

---

## 🏗️ Build Instructions

### 1. Clone the Repository
    git clone https://github.com/sussybocca/TMNT-OS.git
    cd TMNT-OS

### 2. Build the ISO
    make

This will:
- Compile the kernel and all drivers
- Auto-discover and compile all applications
- Embed knowledge files into the binary
- Create tmnt-os.iso

### 3. Run in QEMU
    make run

Or manually:
    qemu-system-x86_64 -cdrom tmnt-os.iso -audiodev pa,id=sb16audio -device sb16,audiodev=sb16audio

### 4. Boot on Real Hardware
Burn tmnt-os.iso to a USB drive:
    sudo dd if=tmnt-os.iso of=/dev/sdX bs=4M status=progress

Or use **Rufus** (Windows): https://rufus.ie/

---

## 🎮 Usage

### Desktop Mode
- Click desktop icons to launch apps
- Use turtle cursor to navigate
- Type 'term' to enter terminal mode
- Type 'gui' to return to desktop

### Terminal Commands
    help      - Show available commands
    ls        - List directory contents
    mkdir     - Create directory
    rm        - Remove file
    cat       - Display file contents
    edit      - Edit/create file
    run       - Execute program
    clear     - Clear screen
    cowabunga - Turtle power!
    term      - Enter terminal mode
    gui       - Return to GUI
    files     - Open file manager
    turtle    - TM customization commands

### TM Commands (in terminal)
    shell-shock HEX  - Change background color
    mask-up COLOR    - Change theme
    bg-style 0-9     - Change background style
    music-list       - List available music
    play             - Play music
    debug-fs         - Show filesystem info

### TMNT Bot Commands
    help              - Show what the bot can do
    list knowledge    - List all knowledge topics
    create app NAME   - Generate a new app
    reload knowledge  - Reload knowledge files
    edit file PATH    - Check/load a file

---

## 🤖 TMNT Bot AI System

The AI assistant is a learning system that:

1. **Background Service** - Runs as a kernel driver watching all I/O
2. **Pattern Learning** - Records every command and its output
3. **Persistent Memory** - Saves learned patterns to filesystem
4. **GUI Chat App** - User-facing chat interface
5. **Knowledge Files** - Loadable .kn files for pre-trained knowledge
6. **Code Generation** - Can create new app source files

The bot gets smarter the more you use the OS!

---

## 🎵 Adding Music

1. Convert audio to 8-bit unsigned PCM, 8000Hz, mono, raw format:
    ffmpeg -i song.mp3 -acodec pcm_u8 -ar 8000 -ac 1 song.raw

2. Place in /user/music/ on the TMNT OS filesystem

3. Use 'play' command or Sound Studio app to play

---

## 🧩 Creating Apps with TAM

TAM (Turtle App Markup) is a simple language for creating apps:

    #app MyCoolApp
    #icon "🎮"
    #window 600 400
    #draw
        fill_rect 0,0,600,400 0x001A0A
        text "Welcome!" 20,40 0x00FF00
    #click
        text "Clicked!" 20,80 0xFFFF00
    #key
        if key='q' end

Build with Ctrl+S in the TAM App Creator!

---

## 🐛 Known Issues

- **Audio Blocking** - SB16 playback blocks during audio (hardware limitation)
- **No Networking** - This is an offline-only OS
- **Memory Limit** - 32-bit, limited by available RAM
- **No USB Support** - PS/2 input only
- **No Compiler in OS** - Apps must be pre-compiled

---

## 🗺️ Roadmap

- [ ] Networking stack (TCP/IP)
- [ ] USB HID support
- [ ] ELF executable loader
- [ ] Multi-tasking / processes
- [ ] Virtual memory / paging
- [ ] More filesystem features
- [ ] Additional audio formats
- [ ] Better AI with more learning capabilities
- [ ] TAM bytecode interpreter

---

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a Pull Request

All contributions must maintain the TMNT theme and provide credit to the original author.

---

## 📄 License

This project is open source under the MIT License with Attribution.

See LICENSE for full details.

**You must provide credit to "sussybocca" and link to https://github.com/sussybocca/TMNT-OS in any use of this code.**

---

## 🙏 Credits

- **sussybocca** - Creator and main developer
- **Teenage Mutant Ninja Turtles** - Inspiration (Cowabunga!)
- **OSDev Community** - Wiki and forums for OS development knowledge
- **QEMU Team** - For the excellent emulator
- **GNU Project** - GCC, GRUB, and other essential tools

---

## 📚 Resources

- [OSDev Wiki](https://wiki.osdev.org/) - Operating system development
- [GRUB Manual](https://www.gnu.org/software/grub/manual/)
- [NASM Documentation](https://www.nasm.us/doc/)
- [QEMU Documentation](https://www.qemu.org/docs/master/)
- [Sound Blaster 16 Programming](http://www.osdever.net/tutorials/view/programming-the-sound-blaster-16)
- [VBE Specification](https://wiki.osdev.org/VESA_Video_Modes)

---

## ⭐ Star History

If you find this project interesting, please star the repository!

---

**🐢 COWABUNGA! 🍕**

*Built with turtle power and late-night pizza in the sewer lair.*
