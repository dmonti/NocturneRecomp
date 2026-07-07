#!/usr/bin/env python3
"""Rebuild default.xex as a plain, unencrypted/uncompressed XEX2 image, so
its data (string tables, etc.) can be hex-edited directly without redoing
AES/basic-compression on every change.

XexModule::ReadImage (rexglue-sdk/src/system/xex_module.cpp) supports
encryption_type=NONE + compression_type=NONE natively (ReadImageUncompressed):
it just memcpy's the file body straight into guest memory at REX_IMAGE_BASE.
No signature/hash check runs on this path (RSA signature checking only
happens for .xexp patch application, not the base image load), so this is a
legitimate, supported load path -- not a bypass of anything.

Usage:
    python scripts/re/rebuild_xex_unencrypted.py [assets/default.xex] out.xex
    python scripts/re/rebuild_xex_unencrypted.py [assets/default.xex] out.xex --patch 0x2064f8=BUG
"""
import argparse
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


def decrypt_xex_image(data, gi):
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
    return bytes(out), header_size, ffi_off


def encode_big_font(word: str) -> bytes:
    """Encode a name-table word the way this title's font stores it: first
    letter as ASCII-0x20, remaining letters plain uppercase ASCII. See
    extracted/README.md for how this was reverse-engineered."""
    b = bytearray(word.upper().encode("ascii"))
    b[0] -= 0x20
    return bytes(b)


def apply_patch(image: bytearray, spec: str):
    """spec is 'ADDR=TEXT', e.g. '0x2064f8=BUG'. TEXT is encoded with the
    same big-font-first-letter scheme as the original table entry it
    overwrites; caller is responsible for picking a replacement no longer
    than the original so following fields aren't disturbed."""
    addr_str, text = spec.split("=", 1)
    addr = int(addr_str, 0)
    encoded = encode_big_font(text)
    image[addr : addr + len(encoded)] = encoded
    print(f"patched {len(encoded)} bytes at {hex(addr)}: {text!r} -> {encoded.hex()}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("xex_in", nargs="?", default=os.path.join(ROOT, "assets", "default.xex"))
    parser.add_argument("xex_out")
    parser.add_argument("--patch", action="append", default=[], help="ADDR=TEXT, repeatable")
    args = parser.parse_args()

    gi = _load_gen_icon()
    data = open(args.xex_in, "rb").read()
    if data[:4] != b"XEX2":
        raise RuntimeError(f"{args.xex_in} is not an XEX2 file")

    image, header_size, ffi_off = decrypt_xex_image(data, gi)
    image = bytearray(image)

    for spec in args.patch:
        apply_patch(image, spec)

    header = bytearray(data[:header_size])
    # file-format-info: { info_size:u32, encryption_type:u16, compression_type:u16, ... }
    struct.pack_into(">H", header, ffi_off + 4, gi.XEX_ENCRYPTION_NONE)
    struct.pack_into(">H", header, ffi_off + 6, gi.XEX_COMPRESSION_NONE)

    with open(args.xex_out, "wb") as f:
        f.write(header)
        f.write(image)
    print(f"wrote {len(header) + len(image)} bytes to {args.xex_out} "
          f"(header {len(header)} + plain image {len(image)}, encryption=NONE compression=NONE)")


if __name__ == "__main__":
    main()
