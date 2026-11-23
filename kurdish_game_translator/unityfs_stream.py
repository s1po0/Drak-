"""Low-memory streaming extractor & repacker for huge UnityFS AssetBundles (.ab).

Avoids loading multi-gigabyte .resS/.resource stream blobs into RAM by unpacking
only the serialized metadata CAB file (~91 MB) and copying remaining compressed
blocks verbatim when repacking.
"""

from __future__ import annotations

import lz4.block
import lzma
import struct
from pathlib import Path
from typing import List, Tuple

import UnityPy
import UnityPy.streams

BLOCK_SIZE = 131072  # 128 KiB standard UnityFS block size


def _decompress_block(raw: bytes, uncompressed_size: int, flags: int) -> bytes:
    comp_type = flags & 0x3F
    if comp_type in (2, 3):
        return lz4.block.decompress(raw, uncompressed_size=uncompressed_size)
    if comp_type == 1:
        return lzma.decompress(raw)
    return raw


def _compress_block(data: bytes) -> Tuple[bytes, int]:
    comp = lz4.block.compress(data, mode="high_compression", store_size=False)
    if len(comp) < len(data):
        return comp, 3  # LZ4HC
    return data, 0      # Uncompressed


def _write_cstring(buf: bytearray, text: str) -> None:
    buf.extend(text.encode("utf-8"))
    buf.append(0)


def _align16(buf: bytearray) -> None:
    rem = len(buf) % 16
    if rem:
        buf.extend(b"\x00" * (16 - rem))


def parse_unityfs_header(f) -> dict:
    r = UnityPy.streams.EndianBinaryReader(f)
    sig = r.read_string_to_null()
    if sig != "UnityFS":
        raise ValueError(f"Expected UnityFS bundle, got {sig!r}")
    ver = r.read_int()
    unity_ver = r.read_string_to_null()
    unity_rev = r.read_string_to_null()
    total_size = r.read_long()
    comp_sz = r.read_int()
    uncomp_sz = r.read_int()
    flags = r.read_int()
    if ver >= 7:
        r.align_stream(16)
    comp_data = r.read_bytes(comp_sz)
    if flags & 0x200:
        r.align_stream(16)
    blocks_start = r.Position

    info_bytes = _decompress_block(comp_data, uncomp_sz, flags)
    ir = UnityPy.streams.EndianBinaryReader(info_bytes)
    uncomp_hash = ir.read_bytes(16)
    block_count = ir.read_int()
    blocks = [(ir.read_int(), ir.read_int(), ir.read_u_short()) for _ in range(block_count)]
    node_count = ir.read_int()
    nodes = [
        (ir.read_long(), ir.read_long(), ir.read_int(), ir.read_string_to_null())
        for _ in range(node_count)
    ]
    return {
        "sig": sig,
        "ver": ver,
        "unity_ver": unity_ver,
        "unity_rev": unity_rev,
        "total_size": total_size,
        "flags": flags,
        "blocks_start": blocks_start,
        "uncomp_hash": uncomp_hash,
        "blocks": blocks,
        "nodes": nodes,
    }


def extract_primary_cab(bundle_path: Path, out_cab_path: Path) -> dict:
    """Extract only node[0] (the serialized CAB assets file) to out_cab_path."""
    with bundle_path.open("rb") as f:
        meta = parse_unityfs_header(f)
        f.seek(meta["blocks_start"])
        target_size = meta["nodes"][0][1]
        written = 0
        out_cab_path.parent.mkdir(parents=True, exist_ok=True)
        with out_cab_path.open("wb") as out:
            for u_sz, c_sz, b_flags in meta["blocks"]:
                raw = f.read(c_sz)
                dec = _decompress_block(raw, u_sz, b_flags)
                need = min(len(dec), target_size - written)
                out.write(dec[:need])
                written += need
                if written >= target_size:
                    break
    return meta


def repack_bundle_with_cab(
    source_bundle: Path,
    modified_cab_path: Path,
    output_bundle: Path,
) -> dict:
    """Repack a UnityFS bundle replacing node[0] while copying remaining blocks verbatim."""
    new_cab_size = modified_cab_path.stat().st_size

    with source_bundle.open("rb") as src:
        meta = parse_unityfs_header(src)
        old_nodes = meta["nodes"]
        old_blocks = meta["blocks"]
        old_cab_size = old_nodes[0][1]

        # Find which source block straddles old_cab_size
        cum = 0
        straddle_idx = 0
        straddle_offset = 0
        src.seek(meta["blocks_start"])
        for idx, (u_sz, c_sz, b_flags) in enumerate(old_blocks):
            if cum + u_sz > old_cab_size:
                straddle_idx = idx
                straddle_offset = old_cab_size - cum
                break
            src.seek(c_sz, 1)
            cum += u_sz
        else:
            straddle_idx = len(old_blocks)
            straddle_offset = 0

        # Build new blocks list & stage compressed payload to a temporary data file
        tmp_data_path = output_bundle.with_suffix(".tmp_blocks")
        output_bundle.parent.mkdir(parents=True, exist_ok=True)
        new_blocks: List[Tuple[int, int, int]] = []

        with tmp_data_path.open("wb") as data_out:
            # 1. Compress modified CAB in 128 KiB blocks
            with modified_cab_path.open("rb") as cab_f:
                while True:
                    chunk = cab_f.read(BLOCK_SIZE)
                    if not chunk:
                        break
                    comp, cflag = _compress_block(chunk)
                    new_blocks.append((len(chunk), len(comp), cflag))
                    data_out.write(comp)

            # 2. Handle straddle block tail (first bytes of node[1]) if any
            if straddle_idx < len(old_blocks):
                u_sz, c_sz, b_flags = old_blocks[straddle_idx]
                raw = src.read(c_sz)
                dec = _decompress_block(raw, u_sz, b_flags)
                tail = dec[straddle_offset:]
                if tail:
                    comp, cflag = _compress_block(tail)
                    new_blocks.append((len(tail), len(comp), cflag))
                    data_out.write(comp)

                # 3. Verbatim raw-copy all remaining compressed blocks!
                for u_sz, c_sz, b_flags in old_blocks[straddle_idx + 1 :]:
                    raw_block = src.read(c_sz)
                    if len(raw_block) != c_sz:
                        raise IOError("Unexpected EOF reading source bundle blocks")
                    new_blocks.append((u_sz, c_sz, b_flags))
                    data_out.write(raw_block)

        # Shift offsets of node[1..] by (new_cab_size - old_cab_size)
        delta = new_cab_size - old_cab_size
        new_nodes = [(0, new_cab_size, old_nodes[0][2], old_nodes[0][3])]
        for off, n_sz, n_flags, n_path in old_nodes[1:]:
            new_nodes.append((off + delta, n_sz, n_flags, n_path))

        # Serialize blocksInfo
        info_buf = bytearray()
        info_buf.extend(meta["uncomp_hash"])
        info_buf.extend(struct.pack(">i", len(new_blocks)))
        for u_sz, c_sz, b_flags in new_blocks:
            info_buf.extend(struct.pack(">IIH", u_sz, c_sz, b_flags))
        info_buf.extend(struct.pack(">i", len(new_nodes)))
        for off, n_sz, n_flags, n_path in new_nodes:
            info_buf.extend(struct.pack(">qqI", off, n_sz, n_flags))
            _write_cstring(info_buf, n_path)

        uncomp_info_bytes = bytes(info_buf)
        comp_info_bytes = lz4.block.compress(
            uncomp_info_bytes, mode="high_compression", store_size=False
        )
        new_flags = (meta["flags"] & ~0x3F) | 3  # LZ4HC info compression

        # Build header
        hdr = bytearray()
        _write_cstring(hdr, meta["sig"])
        hdr.extend(struct.pack(">i", meta["ver"]))
        _write_cstring(hdr, meta["unity_ver"])
        _write_cstring(hdr, meta["unity_rev"])
        size_pos = len(hdr)
        hdr.extend(struct.pack(">q", 0))  # placeholder for total_size
        hdr.extend(struct.pack(">iii", len(comp_info_bytes), len(uncomp_info_bytes), new_flags))
        if meta["ver"] >= 7:
            _align16(hdr)
        hdr.extend(comp_info_bytes)
        if new_flags & 0x200:
            _align16(hdr)

        blocks_data_size = tmp_data_path.stat().st_size
        total_size = len(hdr) + blocks_data_size
        struct.pack_into(">q", hdr, size_pos, total_size)

        with output_bundle.open("wb") as out_f, tmp_data_path.open("rb") as data_in:
            out_f.write(hdr)
            while True:
                buf = data_in.read(4 * 1024 * 1024)
                if not buf:
                    break
                out_f.write(buf)

        tmp_data_path.unlink(missing_ok=True)

    return {
        "output_bundle": str(output_bundle),
        "total_size": total_size,
        "old_cab_size": old_cab_size,
        "new_cab_size": new_cab_size,
        "blocks": len(new_blocks),
    }
