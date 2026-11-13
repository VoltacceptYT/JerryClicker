#!/usr/bin/env python3
"""Convert the game music (MP3) into the Nintendo DS's native sound-hardware format.

    python3 tools/convert_music.py [--rate 22050] [--input assets/music/JerryUnited.mp3]

Produces
    assets/music/jerry_united.adpcm   raw DS IMA-ADPCM stream (embedded in the ROM by source/music_data.s)
    source/music_info.h               sample rate / loop point constants used by source/audio.c
    (optional) --preview out.wav      what the DS will actually play, decoded back to 16-bit PCM

Needs: ffmpeg on PATH and numpy.  The generated files are committed, so the normal
build (make / GitHub Actions) does NOT need either of them.

Format notes (GBATEK "DS Sound Channel Formats"):
  * word 0 is a header: bits 0-15 = initial PCM16 predictor, bits 16-22 = step index (0..88)
  * after that, one 4-bit IMA-ADPCM code per sample, low nibble first
  * total size must be a multiple of 4 bytes
  * a looping ADPCM voice must loop at word >= 1 (never inside the header)
"""
import argparse
import os
import subprocess
import sys
import wave

import numpy as np

STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767,
]
INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8]

LOOP_WORD = 1  # loop right after the 1-word header


def decode_pcm(path, rate):
    """MP3 -> mono float32 at `rate` Hz (ffmpeg does the downmix + band-limited resample)."""
    cmd = ["ffmpeg", "-hide_banner", "-v", "error", "-i", path,
           "-ac", "1", "-ar", str(rate), "-f", "f32le", "-"]
    raw = subprocess.run(cmd, check=True, stdout=subprocess.PIPE).stdout
    return np.frombuffer(raw, dtype="<f4").astype(np.float64)


def trim_tail(x, rate, floor=0.003, keep=0.15):
    """Drop trailing near-silence so the loop restarts promptly (keep a short decay)."""
    loud = np.where(np.abs(x) > floor)[0]
    if len(loud) == 0:
        return x
    end = min(len(x), loud[-1] + int(keep * rate))
    return x[:end]


def encode_ima(pcm16):
    """Encode int PCM16 samples to the DS hardware ADPCM stream. Returns (bytes, decoded_pcm)."""
    n = len(pcm16)
    n += (-n) % 8  # pad to a whole number of 32-bit words
    s = list(pcm16) + [pcm16[-1]] * (n - len(pcm16))

    pred = s[0]
    index = 0
    header = (pred & 0xFFFF) | (index << 16)
    out = bytearray(header.to_bytes(4, "little"))
    dec = [pred] * n

    step_tab, idx_tab = STEP_TABLE, INDEX_TABLE
    lo = None
    for i in range(n):
        step = step_tab[index]
        diff = s[i] - pred
        code = 0
        if diff < 0:
            code = 8
            diff = -diff
        if diff >= step:
            code |= 4
            diff -= step
        if diff >= step >> 1:
            code |= 2
            diff -= step >> 1
        if diff >= step >> 2:
            code |= 1
        # Reconstruct exactly as the DS hardware does (each term floored separately)
        vp = step >> 3
        if code & 1:
            vp += step >> 2
        if code & 2:
            vp += step >> 1
        if code & 4:
            vp += step
        pred = max(-0x7FFF, pred - vp) if code & 8 else min(0x7FFF, pred + vp)
        index += idx_tab[code & 7]
        index = 0 if index < 0 else 88 if index > 88 else index
        dec[i] = pred
        if lo is None:
            lo = code
        else:
            out.append(lo | (code << 4))
            lo = None
    assert len(out) % 4 == 0
    return bytes(out), np.array(dec, dtype=np.int16)


def decode_ima(blob):
    """Independent reference decoder (follows GBATEK) used to verify the encoder."""
    header = int.from_bytes(blob[:4], "little")
    pred = header & 0xFFFF
    pred = pred - 0x10000 if pred & 0x8000 else pred
    index = (header >> 16) & 0x7F
    res = []
    for byte in blob[4:]:
        for code in (byte & 15, byte >> 4):
            step = STEP_TABLE[index]
            d = step // 8
            if code & 1:
                d += step // 4
            if code & 2:
                d += step // 2
            if code & 4:
                d += step
            pred = max(pred - d, -0x7FFF) if code & 8 else min(pred + d, 0x7FFF)
            index = min(88, max(0, index + INDEX_TABLE[code & 7]))
            res.append(pred)
    return np.array(res, dtype=np.int16)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--input", default=os.path.join(root, "assets/music/JerryUnited.mp3"))
    ap.add_argument("--rate", type=int, default=22050,
                    help="playback rate in Hz (default 22050 = ~1.5 MB for 2:18; 16000 = ~1.1 MB; 32000 = ~2.2 MB)")
    ap.add_argument("--peak", type=float, default=0.97, help="normalise peak to this fraction of full scale")
    ap.add_argument("--no-trim", action="store_true", help="keep trailing silence")
    ap.add_argument("--preview", help="also write a WAV of the decoded ADPCM so you can hear the DS result")
    args = ap.parse_args()

    x = decode_pcm(args.input, args.rate)
    if not args.no_trim:
        x = trim_tail(x, args.rate)
    x *= args.peak / max(np.abs(x).max(), 1e-9)
    pcm = np.clip(np.round(x * 32767), -32767, 32767).astype(np.int16)

    blob, dec = encode_ima(pcm.tolist())

    ref = decode_ima(blob)
    assert np.array_equal(ref, dec), "encoder/decoder mismatch"

    n = len(pcm)
    err = pcm.astype(np.float64) - dec[:n]
    snr = 10 * np.log10((pcm.astype(np.float64) ** 2).mean() / max((err ** 2).mean(), 1e-9))

    out_bin = os.path.join(root, "assets/music/jerry_united.adpcm")
    os.makedirs(os.path.dirname(out_bin), exist_ok=True)
    with open(out_bin, "wb") as f:
        f.write(blob)

    with open(os.path.join(root, "source/music_info.h"), "w") as f:
        f.write("// Generated by tools/convert_music.py - do not edit.\n")
        f.write("#ifndef MUSIC_INFO_H\n#define MUSIC_INFO_H\n\n")
        f.write(f"#define MUSIC_RATE      {args.rate}   // Hz\n")
        f.write(f"#define MUSIC_LOOP_WORD {LOOP_WORD}       // ADPCM loop start (32-bit words, after the header)\n")
        f.write(f"#define MUSIC_BYTES     {len(blob)}\n\n#endif\n")

    if args.preview:
        with wave.open(args.preview, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(args.rate)
            w.writeframes(dec[:n].tobytes())

    print(f"{n / args.rate:.2f} s @ {args.rate} Hz mono -> {len(blob)} bytes "
          f"({len(blob) / 1048576:.2f} MiB), ADPCM SNR {snr:.1f} dB")


if __name__ == "__main__":
    sys.exit(main())
