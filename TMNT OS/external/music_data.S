.section .rodata.music, "a"
.global tmnt_theme_start
.global tmnt_theme_end
tmnt_theme_start:
.incbin "assets/Music/tmnt_theme.raw"
tmnt_theme_end:

.section .rodata.music_size, "a"
.global tmnt_theme_size
.align 4
tmnt_theme_size:
    .long tmnt_theme_end - tmnt_theme_start
