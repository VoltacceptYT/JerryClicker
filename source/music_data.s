@ Embeds the DS IMA-ADPCM music stream (made by tools/convert_music.py) into the ROM.
@ 32-byte alignment: the ARM7 sound hardware needs a word-aligned source, and cache-line
@ alignment keeps DC_FlushRange() exact.
    .section .rodata
    .balign 32
    .global jerry_music_adpcm
jerry_music_adpcm:
    .incbin "assets/music/jerry_united.adpcm"
    .global jerry_music_adpcm_end
jerry_music_adpcm_end:
    .balign 4
