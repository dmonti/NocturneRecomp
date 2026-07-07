#!/usr/bin/env python3
"""Decrypt + decompress assets/default.xex to its plain PE image bytes.

Reuses the exact AES-128-CBC (retail common key -> per-title session key)
and XEX_COMPRESSION_BASIC block-expansion logic that scripts/gen-icon.py
already implements for pulling the title icon out of the XEX resource
table, but runs it over the *whole* file instead of one resource range.
Useful for offline searching (grep, strings) for anything already found
live in a running process's guest memory via re/scan_guest_memory.py --
the data is the same either way, default.xex is just AES-encrypted and
"basic" compressed on disk, never actually missing.

Usage:
    python scripts/re/dump_xex_image.py [assets/default.xex] out.bin
"""
import importlib.util
import os
import struct
import sys

try:
    from Crypto.Cipher import AES
except ImportError:
    print("error: pip install pycryptodome", file=sys.stderr)
    sys.exit(1)

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def _load_gen_icon():
    spec = importlib.util.spec_from_file_location("gen_icon", os.path.join(ROOT, "scripts", "gen-icon.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def decrypt_xex_image(xex_path, gi):
    data = open(xex_path, "rb").read()
    if data[:4] != b"XEX2":
        raise RuntimeError(f"{xex_path} is not an XEX2 file")

    header_size = struct.unpack_from(">I", data, 0x8)[0]
    security_offset = struct.unpack_from(">I", data, 0x10)[0]
    opt_headers = gi._read_opt_headers(data)

    ffi_off = opt_headers[gi.XEX_HEADER_FILE_FORMAT_INFO]
    info_size, encryption_type, compression_type = struct.unpack_from(">IHH", data, ffi_off)
    if encryption_type != gi.XEX_ENCRYPTION_NORMAL or compression_type != gi.XEX_COMPRESSION_BASIC:
        raise RuntimeError(f"unsupported encryption={encryption_type} compression={compression_type}")

    blocks = gi._read_basic_blocks(data, ffi_off, info_size)
    encrypted_key = data[security_offset + 0x150 : security_offset + 0x160]
    session_key = gi._aes128_decrypt_block(gi.RETAIL_KEY, encrypted_key)

    body = data[header_size:]
    total_data = sum(b[0] for b in blocks)
    cipher = AES.new(session_key, AES.MODE_CBC, iv=bytes(16))
    plaintext = cipher.decrypt(body[:total_data])

    out = bytearray()
    di = 0
    for data_size, zero_size in blocks:
        out += plaintext[di : di + data_size]
        out += b"\x00" * zero_size
        di += data_size
    return bytes(out)


def main():
    args = sys.argv[1:]
    if len(args) == 1:
        xex_path, out_path = os.path.join(ROOT, "assets", "default.xex"), args[0]
    elif len(args) == 2:
        xex_path, out_path = args
    else:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    gi = _load_gen_icon()
    out = decrypt_xex_image(xex_path, gi)
    with open(out_path, "wb") as f:
        f.write(out)
    print(f"wrote {len(out)} bytes to {out_path}")


if __name__ == "__main__":
    main()
