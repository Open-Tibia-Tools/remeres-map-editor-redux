#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tests for tools/tibia_data.py.

Run with:
    python -m unittest discover -s tools -p "test_*.py" -v
    python tools/test_tibia_data.py -v

The integration tests use the sample client in Tests1098/ and are skipped when
those files are not present.
"""

from __future__ import annotations

import hashlib
import io
import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent
TOOL_PATH = TOOLS_DIR / "tibia_data.py"
sys.path.insert(0, str(TOOLS_DIR))

import tibia_data as td  # noqa: E402

TEST_CLIENT_DIR = REPO_ROOT / "Tests1098"
HAS_TEST_CLIENT = all(
    (TEST_CLIENT_DIR / name).exists()
    for name in ("Tibia.dat", "Tibia.spr", "items.otb")
)


# --------------------------------------------------------------------------
# Synthetic file builders
# --------------------------------------------------------------------------


def flags_bytes(entries):
    """Encode a DAT flag block.  entries = [(flag, payload_bytes), ...]."""
    out = bytearray()
    for code, payload in entries:
        out.append(code)
        out += payload
    out.append(0xFF)
    return bytes(out)


def pattern_bytes(
    width=1,
    height=1,
    layers=1,
    pattern_x=1,
    pattern_y=1,
    pattern_z=1,
    frames=1,
    sprite_ids=(),
    extended=True,
    exact_size=None,
    durations=None,
    animation_mode=0,
    loop_count=0,
    start_frame=0,
    group_type=None,
    read_pattern_z=True,
):
    out = bytearray()
    if group_type is not None:
        out.append(group_type)
    out.append(width)
    out.append(height)
    if width > 1 or height > 1:
        out.append(32 if exact_size is None else exact_size)
    out.append(layers)
    out.append(pattern_x)
    out.append(pattern_y)
    if read_pattern_z:
        out.append(pattern_z)
    out.append(frames)
    if frames > 1 and durations is not None:
        out.append(animation_mode)
        out += struct.pack("<i", loop_count)
        out += struct.pack("<b", start_frame)
        for low, high in durations:
            out += struct.pack("<II", low, high)
    width_bytes = 4 if extended else 2
    for sprite_id in sprite_ids:
        out += sprite_id.to_bytes(width_bytes, "little")
    return bytes(out)


def build_dat(
    dat_signature,
    items=(),
    outfits=(),
    effects=(),
    missiles=(),
    items_count_override=None,
):
    """Build a DAT file.

    items/outfits/effects/missiles are lists of raw record bytes (flags +
    pattern).  Item ids start at 100, so the header item count is
    99 + len(items).
    """
    items_count = 99 + len(items) if items_count_override is None else items_count_override
    out = bytearray()
    out += struct.pack("<I", dat_signature)
    out += struct.pack(
        "<HHHH", items_count, len(outfits), len(effects), len(missiles)
    )
    for record in list(items) + list(outfits) + list(effects) + list(missiles):
        out += record
    return bytes(out)


def build_item(flag_entries, sprite_ids, extended=True, **pattern_kwargs):
    return flags_bytes(flag_entries) + pattern_bytes(
        sprite_ids=sprite_ids, extended=extended, **pattern_kwargs
    )


def build_outfit(flag_entries, groups, extended=True, read_pattern_z=True):
    """groups = [(group_type, pattern_kwargs), ...]"""
    out = bytearray(flags_bytes(flag_entries))
    out.append(len(groups))
    for group_type, kwargs in groups:
        kwargs = dict(kwargs)
        kwargs["extended"] = extended
        kwargs["read_pattern_z"] = read_pattern_z
        out += pattern_bytes(group_type=group_type, **kwargs)
    return bytes(out)


# -- OTB -------------------------------------------------------------------


def otb_escape(data):
    out = bytearray()
    for value in data:
        if value in (td.OTB_ESCAPE, td.OTB_NODE_START, td.OTB_NODE_END):
            out.append(td.OTB_ESCAPE)
        out.append(value)
    return bytes(out)


def otb_prop(attr, payload):
    return bytes([attr]) + struct.pack("<H", len(payload)) + payload


def otb_node(node_type, data):
    return (
        bytes([td.OTB_NODE_START, node_type])
        + otb_escape(data)
        + bytes([td.OTB_NODE_END])
    )


def build_otb(items, major=3, minor=57, build=62, csd=b"OTB 3.57.62-10.98"):
    """items = [(group, flags, attribute_bytes), ...]"""
    version_payload = (
        struct.pack("<III", major, minor, build) + csd.ljust(128, b"\x00")[:128]
    )
    root_data = struct.pack("<I", 0) + otb_prop(0x01, version_payload)
    out = bytearray(struct.pack("<I", 0))
    # ObjectBuilder writes the OTB flat: root node start + data, then the item
    # nodes, and a single root NODE_END at the end of the file.
    out += bytes([td.OTB_NODE_START, 0x00]) + otb_escape(root_data)
    for group, flags, attributes in items:
        out += otb_node(group, struct.pack("<I", flags) + attributes)
    out += bytes([td.OTB_NODE_END])  # close root node
    return bytes(out)


def otb_attrs(server_id=None, client_id=None, name=None, light=None, sprite_hash=None,
              extra=b""):
    data = bytearray()
    if server_id is not None:
        data += otb_prop(0x10, struct.pack("<H", server_id))
    if client_id is not None:
        data += otb_prop(0x11, struct.pack("<H", client_id))
    if name is not None:
        data += otb_prop(0x12, name)
    if light is not None:
        data += otb_prop(0x2A, struct.pack("<HH", light[0], light[1]))
    if sprite_hash is not None:
        data += otb_prop(0x20, sprite_hash)
    data += extra
    return bytes(data)


# -- SPR -------------------------------------------------------------------


def encode_sprite_chunks(pixels, transparent):
    """Encode 1024 RGBA pixels as a SPR chunk stream.

    Uses one transparent run followed by one colored run, repeating when the
    image alternates more than once (tests use simple patterns).
    """
    channels = 4 if transparent else 3
    out = bytearray()
    index = 0
    size = len(pixels) // 4
    while index < size:
        transparent_run = 0
        while index < size and pixels[index * 4 + 3] == 0:
            transparent_run += 1
            index += 1
        colored_run = 0
        while index < size and pixels[index * 4 + 3] != 0:
            colored_run += 1
            index += 1
        out += struct.pack("<HH", transparent_run, colored_run)
        for _ in range(colored_run):
            pass
        start = (index - colored_run) * 4
        for pixel in range(colored_run):
            offset = start + pixel * 4
            out += bytes(pixels[offset : offset + 3])
            if transparent:
                out.append(pixels[offset + 3])
    return bytes(out)


def build_spr(signature, sprites, extended=True):
    """sprites = [None (empty) or chunk-stream bytes]."""
    header_size = 8 if extended else 6
    count = len(sprites)
    table = bytearray()
    payload = bytearray()
    address = header_size + count * 4
    for chunks in sprites:
        if chunks is None:
            table += struct.pack("<I", 0)
            continue
        table += struct.pack("<I", address)
        blob = bytes([0, 0, 0]) + struct.pack("<H", len(chunks)) + chunks
        payload += blob
        address += len(blob)
    out = bytearray(struct.pack("<I", signature))
    out += struct.pack("<I" if extended else "<H", count)
    out += table + payload
    return bytes(out)


def solid_pixels(color=(10, 20, 30, 255), count=1024):
    return bytes(bytes(color)) * count


def transparent_pixels(count=1024):
    return bytes(4) * count


# --------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------


def run_cli(args, expect_ok=True):
    result = subprocess.run(
        [sys.executable, str(TOOL_PATH)] + [str(a) for a in args],
        capture_output=True,
        text=True,
    )
    document = json.loads(result.stdout)
    if expect_ok:
        if result.returncode != 0:
            raise AssertionError(
                "CLI failed (%d): %s\n%s"
                % (result.returncode, result.stdout, result.stderr)
            )
    return result.returncode, document


def png_pixels(data):
    """Minimal PNG reader for tests: returns (width, height, RGBA rows)."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError("not a PNG")
    pos = 8
    width = height = None
    idat = bytearray()
    while pos < len(data):
        length = struct.unpack_from(">I", data, pos)[0]
        tag = data[pos + 4 : pos + 8]
        payload = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if tag == b"IHDR":
            width, height, depth, color_type = struct.unpack(">IIBB", payload[:10])
            if depth != 8 or color_type != 6:
                raise AssertionError("expected 8-bit RGBA")
        elif tag == b"IDAT":
            idat += payload
        elif tag == b"IEND":
            break
    raw = zlib.decompress(bytes(idat))
    stride = width * 4
    rows = []
    for y in range(height):
        start = y * (stride + 1)
        if raw[start] != 0:
            raise AssertionError("unexpected PNG filter")
        rows.append(raw[start + 1 : start + 1 + stride])
    return width, height, rows


# --------------------------------------------------------------------------
# Version detection tests
# --------------------------------------------------------------------------


class VersionTests(unittest.TestCase):
    def test_version_table_lookup(self):
        matches = td.find_versions(0x42A3, 0x57BBD603)
        self.assertEqual([row[1] for row in matches], ["10.98"])

    def test_dat_only_lookup_with_shared_signature(self):
        matches = td.find_versions(0x439D5A33, None)
        self.assertEqual([row[1] for row in matches], ["7.60", "7.70"])

    def test_unknown_signature(self):
        self.assertEqual(td.find_versions(0xDEADBEEF, None), [])

    def test_reader_selection(self):
        self.assertEqual(td.pick_reader_kind(730), 1)
        self.assertEqual(td.pick_reader_kind(740), 2)
        self.assertEqual(td.pick_reader_kind(772), 3)
        self.assertEqual(td.pick_reader_kind(854), 4)
        self.assertEqual(td.pick_reader_kind(986), 5)
        self.assertEqual(td.pick_reader_kind(1098), 6)


# --------------------------------------------------------------------------
# DAT tests
# --------------------------------------------------------------------------


class DatTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, name, data):
        path = Path(self.tmp.name) / name
        path.write_bytes(data)
        return path

    def test_parse_item_ids_start_at_100(self):
        item_a = build_item(
            [(0x00, struct.pack("<H", 200))], [101, 102], pattern_x=2
        )
        item_b = build_item([(0x04, b"")], [102, 103], pattern_x=2)
        data = build_dat(0x42A3, items=[item_a, item_b])
        path = self.write("Tibia.dat", data)
        dat = td.DatFile(path)

        header = dat.header()
        self.assertEqual(header["items"], 101)

        features = {
            "extended": True,
            "improvedAnimations": True,
            "frameGroups": True,
        }
        collect = {("item", 100): None, ("item", 101): None}
        header, collected, max_sprite = dat.parse(1098, features, collect=collect)
        self.assertEqual(max_sprite, 103)
        self.assertEqual(sorted(collected), [("item", 100), ("item", 101)])

        first = collected[("item", 100)]
        self.assertTrue(first.flags["isGround"])
        self.assertEqual(first.values["groundSpeed"], 200)
        self.assertEqual(first.frame_groups[0]["spriteIds"], [101, 102])
        self.assertEqual(first.frame_groups[0]["patternX"], 2)

        second = collected[("item", 101)]
        self.assertTrue(second.flags["isContainer"])
        self.assertEqual(second.frame_groups[0]["spriteIds"], [102, 103])

    def test_features_auto_detection(self):
        record = build_item([(0x04, b"")], [5])
        data = build_dat(0x42A3, items=[record])
        path = self.write("Tibia.dat", data)
        dat = td.DatFile(path)
        detection = dat.detect(
            1098,
            {"extended": None, "improvedAnimations": None, "frameGroups": None},
        )
        self.assertTrue(detection["validated"])
        self.assertEqual(
            detection["features"],
            {"extended": True, "improvedAnimations": True, "frameGroups": True},
        )

        # Non-extended variant with a 16-bit sprite id must be detected too.
        record = build_item([], [5], extended=False)
        data = build_dat(0x42A3, items=[record])
        path = self.write("Tibia16.dat", data)
        detection = td.DatFile(path).detect(1098, {})
        self.assertTrue(detection["validated"])
        self.assertFalse(detection["features"]["extended"])

    def test_full_parse_requires_exact_eof(self):
        record = build_item([], [5])
        data = build_dat(0x42A3, items=[record]) + b"\x00"
        path = self.write("Tibia.dat", data)
        dat = td.DatFile(path)
        detection = dat.detect(1098, {})
        self.assertFalse(detection["validated"])

    def test_old_reader_740_pattern_z(self):
        # Reader 2 (7.40 - 7.50) does not read patternZ.
        record = flags_bytes([(0x00, struct.pack("<H", 150))]) + pattern_bytes(
            sprite_ids=[1, 2],
            extended=False,
            read_pattern_z=False,
            pattern_x=2,
        )
        data = build_dat(0x41BF619C, items=[record])
        path = self.write("Tibia.dat", data)
        dat = td.DatFile(path)
        detection = dat.detect(740, {})
        self.assertTrue(detection["validated"])
        self.assertFalse(detection["features"]["extended"])
        self.assertFalse(detection["features"]["improvedAnimations"])

        collect = {("item", 100): None}
        _, collected, _ = dat.parse(740, detection["features"], collect=collect)
        item = collected[("item", 100)]
        self.assertEqual(item.values["groundSpeed"], 150)
        self.assertEqual(item.frame_groups[0]["patternZ"], 1)
        self.assertEqual(item.frame_groups[0]["spriteIds"], [1, 2])

    def test_market_and_bones_flags(self):
        market = struct.pack("<HHHH", 7, 8, 9, 5) + b"rope\x00"
        market += struct.pack("<HH", 4, 20)
        record = build_item(
            [(0x04, b""), (0x22, market), (0x27, struct.pack("<8h", 1, 2, 3, 4, 5, 6, 7, 8))],
            [1],
        )
        data = build_dat(0x42A3, items=[record], items_count_override=100)
        path = self.write("Tibia.dat", data)
        dat = td.DatFile(path)
        collect = {("item", 100): None}
        _, collected, _ = dat.parse(
            1098,
            {"extended": True, "improvedAnimations": True, "frameGroups": True},
            collect=collect,
        )
        item = collected[("item", 100)]
        self.assertTrue(item.flags["isMarketItem"])
        self.assertEqual(item.market_name, "rope\x00")
        self.assertEqual(item.values["marketCategory"], 7)
        self.assertEqual(item.values["marketRestrictLevel"], 20)
        self.assertTrue(item.flags["hasBones"])
        self.assertEqual(item.bones_offsets["north"], {"x": 1, "y": 2})
        self.assertEqual(item.bones_offsets["west"], {"x": 7, "y": 8})

    def test_outfit_frame_groups(self):
        outfit = build_outfit(
            [],
            [
                (0, {"width": 2, "height": 2, "sprite_ids": list(range(10, 14))}),
                (
                    1,
                    {
                        "width": 2,
                        "height": 2,
                        "frames": 2,
                        "durations": [(300, 300), (300, 300)],
                        "sprite_ids": list(range(20, 28)),
                    },
                ),
            ],
        )
        data = build_dat(0x42A3, outfits=[outfit])
        path = self.write("Tibia.dat", data)
        dat = td.DatFile(path)
        collect = {("outfit", 1): None}
        _, collected, _ = dat.parse(
            1098,
            {"extended": True, "improvedAnimations": True, "frameGroups": True},
            collect=collect,
        )
        thing = collected[("outfit", 1)]
        self.assertEqual(len(thing.frame_groups), 2)
        self.assertEqual(thing.frame_groups[0]["type"], 0)
        self.assertEqual(thing.frame_groups[0]["spriteIds"], list(range(10, 14)))
        self.assertEqual(thing.frame_groups[1]["type"], 1)
        self.assertEqual(thing.frame_groups[1]["frames"], 2)
        self.assertEqual(len(thing.frame_groups[1]["frameDurations"]), 2)

    def test_unknown_flag_raises(self):
        record = flags_bytes([(0x3F, b"")]) + pattern_bytes(sprite_ids=[1])
        data = build_dat(0x42A3, items=[record], items_count_override=100)
        path = self.write("Tibia.dat", data)
        dat = td.DatFile(path)
        with self.assertRaises(td.ParseError):
            dat.parse(
                1098,
                {"extended": True, "improvedAnimations": True, "frameGroups": True},
            )


# --------------------------------------------------------------------------
# OTB tests
# --------------------------------------------------------------------------


class OtbTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, data):
        path = Path(self.tmp.name) / "items.otb"
        path.write_bytes(data)
        return path

    def test_parse_items_and_escaping(self):
        name = b"weird\xfd\xfe\xffname"
        data = build_otb(
            [
                (1, 0x02000003, otb_attrs(
                    server_id=100,
                    client_id=930,
                    name=name,
                    light=(3, 156),
                    sprite_hash=bytes(range(16)),
                )),
                (0, 0x00000001 | (1 << 6) | (1 << 8), otb_attrs(server_id=101, client_id=931)),
                (14, 0, otb_attrs(server_id=102)),  # deprecated, no client id
            ]
        )
        otb = td.OtbFile(self.write(data))
        otb.parse()

        self.assertEqual(otb.version_major, 3)
        self.assertEqual(otb.version_minor, 57)
        self.assertEqual(otb.version_build, 62)
        self.assertEqual(otb.csd_version, "OTB 3.57.62-10.98")
        self.assertEqual(len(otb.items), 3)

        first = otb.items[0]
        self.assertEqual(first.server_id, 100)
        self.assertEqual(first.client_id, 930)
        self.assertEqual(first.group_raw, 1)
        self.assertEqual(first.attributes["name"], "weird\u00fd\u00fe\u00ffname")
        self.assertEqual(first.attributes["lightLevel"], 3)
        self.assertEqual(first.attributes["lightColor"], 156)
        self.assertEqual(first.attributes["spriteHash"], bytes(range(16)).hex())
        self.assertTrue(first.model_flags["unpassable"])
        self.assertTrue(first.model_flags["blockMissiles"])
        self.assertTrue(first.model_flags["fullGround"])
        self.assertFalse(first.model_flags["movable"])

        second = otb.items[1]
        self.assertTrue(second.model_flags["movable"])
        self.assertIn("FLOOR_CHANGE_DOWN", second.other_bits)

        third = otb.items[2]
        self.assertEqual(third.group_raw, 14)
        self.assertIsNone(third.client_id)

    def test_attribute_round_trip_raw(self):
        data = build_otb(
            [(0, 0, otb_attrs(server_id=7, client_id=107, extra=otb_prop(0x2B, b"\x03")))]
        )
        otb = td.OtbFile(self.write(data))
        otb.parse()
        item = otb.items[0]
        self.assertEqual(item.attributes["stackOrder"], 3)
        codes = [entry["code"] for entry in item.raw_attributes]
        self.assertEqual(codes, ["0x10", "0x11", "0x2B"])

    def test_missing_server_id_raises(self):
        data = build_otb([(0, 0, otb_attrs(client_id=107))])
        otb = td.OtbFile(self.write(data))
        with self.assertRaises(td.ParseError):
            otb.parse()


# --------------------------------------------------------------------------
# SPR tests
# --------------------------------------------------------------------------


class SprTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self._sprs = []

    def tearDown(self):
        for spr in self._sprs:
            spr.close()
        self.tmp.cleanup()

    def write(self, data, name="Tibia.spr"):
        path = Path(self.tmp.name) / name
        path.write_bytes(data)
        return path

    def open_spr(self, data, name="Tibia.spr"):
        spr = td.SprFile(self.write(data, name))
        self._sprs.append(spr)
        return spr

    def test_decode_rgb_sprite(self):
        pixels = bytearray(transparent_pixels())
        for index in range(4):
            pixels[index * 4 : index * 4 + 4] = bytes([index + 1, 2, 3, 255])
        chunks = encode_sprite_chunks(pixels, transparent=False)
        data = build_spr(0x57BBD603, [chunks], extended=True)
        spr = self.open_spr(data)
        self.assertTrue(spr.detect_extended(1098))
        self.assertEqual(spr.init(True), 1)

        payload, length, address = spr.read_compressed(1)
        self.assertEqual(length, len(chunks))
        self.assertEqual(spr.data_gap(1, address, length), 0)
        decoded, visible, total = spr.decode(payload, transparent=False)
        self.assertEqual(total, 1024)
        self.assertEqual(visible, 4)
        self.assertEqual(decoded[0:4], bytes([1, 2, 3, 255]))
        self.assertEqual(decoded[4:8], bytes([2, 2, 3, 255]))
        self.assertEqual(decoded[16:20], bytes([0, 0, 0, 0]))

    def test_decode_rgba_sprite(self):
        pixels = bytearray(transparent_pixels())
        pixels[0:4] = bytes([50, 60, 70, 128])
        pixels[4:8] = bytes([1, 2, 3, 255])
        chunks = encode_sprite_chunks(pixels, transparent=True)
        data = build_spr(0x57BBD603, [chunks], extended=True)
        spr = self.open_spr(data)
        spr.init(True)
        payload = spr.read_compressed(1)[0]
        decoded, visible, total = spr.decode(payload, transparent=True)
        self.assertEqual(decoded[0:4], bytes([50, 60, 70, 128]))
        self.assertEqual(decoded[4:8], bytes([1, 2, 3, 255]))

    def test_transparency_detection(self):
        rgb = encode_sprite_chunks(solid_pixels(), transparent=False)
        data = build_spr(0x57BBD603, [rgb], extended=True)
        spr = self.open_spr(data, "rgb.spr")
        spr.init(True)
        detected, detail = spr.detect_transparency()
        self.assertIs(detected, False)
        self.assertEqual(detail["confidence"], "validated")

        rgba = encode_sprite_chunks(solid_pixels(), transparent=True)
        data = build_spr(0x57BBD603, [rgba], extended=True)
        spr = self.open_spr(data, "rgba.spr")
        spr.init(True)
        detected, detail = spr.detect_transparency()
        self.assertIs(detected, True)
        self.assertEqual(detail["confidence"], "validated")

    def test_extended_detection_16bit_header(self):
        data = build_spr(0x4C28B721, [encode_sprite_chunks(solid_pixels(), False)], extended=False)
        spr = self.open_spr(data)
        self.assertFalse(spr.detect_extended(860))
        self.assertEqual(spr.init(False), 1)

    def test_empty_sprite_and_out_of_range(self):
        data = build_spr(
            0x57BBD603,
            [None, encode_sprite_chunks(solid_pixels(), False)],
            extended=True,
        )
        spr = self.open_spr(data)
        spr.init(True)
        self.assertIsNone(spr.read_compressed(1))
        with self.assertRaises(td.ParseError):
            spr.read_compressed(3)

    def test_png_roundtrip(self):
        pixels = bytearray(transparent_pixels())
        pixels[0:4] = bytes([10, 20, 30, 255])
        png = td.encode_png(32, 32, bytes(pixels))
        width, height, rows = png_pixels(png)
        self.assertEqual((width, height), (32, 32))
        self.assertEqual(rows[0][0:4], bytes([10, 20, 30, 255]))
        self.assertEqual(rows[1][0:4], bytes(4))

    def test_compose_sheet(self):
        a = bytes([1, 2, 3, 255]) * 1024
        b = bytes([4, 5, 6, 255]) * 1024
        width, height, canvas = td.compose_sheet([a, b], columns=2)
        self.assertEqual((width, height), (64, 32))
        self.assertEqual(canvas[0:4], bytes([1, 2, 3, 255]))
        self.assertEqual(canvas[32 * 4 : 32 * 4 + 4], bytes([4, 5, 6, 255]))


# --------------------------------------------------------------------------
# CLI tests (synthetic files)
# --------------------------------------------------------------------------


class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)

        item_one = build_item(
            [(0x00, struct.pack("<H", 150)), (0x1F, b"")],
            [1, 2],
            pattern_x=2,
        )
        item_two = build_item([(0x04, b"")], [2, 3])
        outfit = build_outfit(
            [],
            [
                (0, {"sprite_ids": [1]}),
                (1, {"frames": 2, "durations": [(500, 500), (500, 500)], "sprite_ids": [1, 1]}),
            ],
        )
        effect = build_item([], [3])
        missile = build_item([], [4])
        # Item ids start at 100: entries for 100 and 101.
        self.dat = self.dir / "Tibia.dat"
        self.dat.write_bytes(
            build_dat(0x42A3, items=[item_one, item_two], outfits=[outfit],
                      effects=[effect], missiles=[missile])
        )

        pixels = bytearray(transparent_pixels())
        pixels[0:4] = bytes([9, 8, 7, 255])
        chunks = encode_sprite_chunks(pixels, transparent=False)
        self.spr = self.dir / "Tibia.spr"
        self.spr.write_bytes(build_spr(0x57BBD603, [chunks, chunks, chunks, chunks]))

        self.otb = self.dir / "items.otb"
        self.otb.write_bytes(
            build_otb(
                [
                    (1, 0x02000003, otb_attrs(server_id=784, client_id=100,
                                                sprite_hash=bytes(range(16)))),
                    (0, 0, otb_attrs(server_id=930, client_id=101)),
                ]
            )
        )

    def tearDown(self):
        self.tmp.cleanup()

    def base(self):
        return ["--dat", self.dat, "--spr", self.spr, "--otb", self.otb]

    def test_info(self):
        code, document = run_cli(self.base() + ["info"])
        self.assertEqual(code, 0)
        self.assertEqual(document["status"], "ok")
        self.assertEqual(document["gameVersion"]["label"], "10.98")
        self.assertEqual(document["counts"]["minItemId"], 100)
        self.assertEqual(document["counts"]["maxItemId"], 101)
        self.assertEqual(document["counts"]["items"], 2)
        self.assertEqual(document["counts"]["outfits"], 1)
        self.assertEqual(document["counts"]["effects"], 1)
        self.assertEqual(document["counts"]["missiles"], 1)
        self.assertEqual(document["counts"]["sprites"], 4)
        self.assertEqual(document["counts"]["otbItems"], 2)
        self.assertTrue(document["features"]["detection"]["datValidated"])
        self.assertEqual(document["result"]["otb"]["version"], "3.57.62")

    def test_client_item(self):
        code, document = run_cli(self.base() + ["client", "100"])
        self.assertEqual(code, 0)
        thing = document["result"]["clientThing"]
        self.assertEqual(thing["id"], 100)
        self.assertEqual(thing["category"], "item")
        self.assertTrue(thing["flags"]["isGround"])
        self.assertTrue(thing["flags"]["isFullGround"])
        self.assertEqual(thing["values"]["groundSpeed"], 150)
        self.assertEqual(thing["spriteIds"], [1, 2])
        self.assertEqual(document["result"]["otbMapping"]["serverIds"], [784])

    def test_client_outfit_effect_missile(self):
        _, document = run_cli(self.base() + ["client", "1", "--category", "outfit"])
        outfit = document["result"]["clientThing"]
        self.assertEqual(len(outfit["frameGroups"]), 2)
        self.assertEqual(outfit["frameGroups"][1]["typeName"], "walking")

        _, document = run_cli(self.base() + ["client", "1", "--category", "effect"])
        self.assertEqual(document["result"]["clientThing"]["category"], "effect")

        _, document = run_cli(self.base() + ["client", "1", "--category", "missile"])
        self.assertEqual(document["result"]["clientThing"]["category"], "missile")

    def test_server_item(self):
        code, document = run_cli(self.base() + ["server", "930"])
        self.assertEqual(code, 0)
        item = document["result"]["serverItem"]
        self.assertEqual(item["serverId"], 930)
        self.assertEqual(item["clientId"], 101)
        self.assertNotIn("name", item["attributes"])
        self.assertEqual(document["result"]["clientThing"]["id"], 101)

    def test_server_without_dat(self):
        code, document = run_cli(["--otb", self.otb, "server", "930"])
        self.assertEqual(code, 0)
        self.assertEqual(document["result"]["serverItem"]["clientId"], 101)
        self.assertNotIn("clientThing", document["result"])

    def test_sprite_and_png_export(self):
        out = self.dir / "sprites"
        code, document = run_cli(["--spr", self.spr, "sprite", "1", "--out", out])
        self.assertEqual(code, 0)
        self.assertFalse(document["result"]["sprite"]["empty"])
        self.assertEqual(document["counts"]["sprites"], 4)
        png_file = Path(document["result"]["pngFile"])
        self.assertTrue(png_file.exists())
        width, height, rows = png_pixels(png_file.read_bytes())
        self.assertEqual((width, height), (32, 32))
        self.assertEqual(rows[0][0:4], bytes([9, 8, 7, 255]))

    def test_dump_sprites_and_sheet(self):
        out = self.dir / "dump"
        code, document = run_cli(
            self.base() + ["client", "100", "--dump-sprites", out, "--sheet"]
        )
        self.assertEqual(code, 0)
        export = document["result"]["spriteExport"]
        self.assertEqual([entry["spriteId"] for entry in export["sprites"]], [1, 2])
        self.assertFalse(export["transparentClient"])
        sheet = Path(export["sheet"])
        width, height, _ = png_pixels(sheet.read_bytes())
        self.assertEqual((width, height), (64, 32))
        for entry in export["sprites"]:
            self.assertTrue(Path(entry["file"]).exists())

    def test_output_file(self):
        out = self.dir / "dumps" / "client_100.json"
        result = subprocess.run(
            [sys.executable, str(TOOL_PATH)]
            + [str(a) for a in self.base()]
            + ["--output", str(out), "client", "100"],
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout.strip(), "")
        document = json.loads(out.read_text(encoding="utf-8"))
        self.assertEqual(document["result"]["clientThing"]["id"], 100)

    def test_id_out_of_range(self):
        code, document = run_cli(self.base() + ["client", "99"], expect_ok=False)
        self.assertEqual(code, 1)
        self.assertEqual(document["status"], "error")
        self.assertIn("out of range 100..101", document["error"])

    def test_server_id_not_found(self):
        code, document = run_cli(self.base() + ["server", "9999"], expect_ok=False)
        self.assertEqual(code, 1)
        self.assertIn("not found", document["error"])

    def test_unknown_version(self):
        bad_dat = self.dir / "bad.dat"
        bad_dat.write_bytes(struct.pack("<I", 0xDEADBEEF) + bytes(8))
        code, document = run_cli(
            ["--dat", bad_dat, "info"], expect_ok=False
        )
        self.assertEqual(code, 1)
        self.assertIn("unknown client version", document["error"])

    def test_force_version(self):
        code, document = run_cli(["--dat", self.dat, "--force-version", "10.98", "info"])
        self.assertEqual(code, 0)
        self.assertEqual(document["gameVersion"]["label"], "10.98")


# --------------------------------------------------------------------------
# Integration tests with the real 10.98 sample client
# --------------------------------------------------------------------------


@unittest.skipUnless(HAS_TEST_CLIENT, "Tests1098 sample client not present")
class Client1098IntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dat = td.DatFile(TEST_CLIENT_DIR / "Tibia.dat")
        cls.header = cls.dat.header()
        cls.detection = cls.dat.detect(1098, {})
        cls.otb = td.OtbFile(TEST_CLIENT_DIR / "items.otb")
        cls.otb.parse()
        cls.spr = td.SprFile(TEST_CLIENT_DIR / "Tibia.spr")
        cls.spr.init(True)

    @classmethod
    def tearDownClass(cls):
        cls.spr.close()

    def entries(self, keys):
        collected = {key: None for key in keys}
        self.dat.parse(1098, self.detection["features"], collect=collected)
        return collected

    def test_header_and_features(self):
        self.assertEqual(self.header["datSignature"], 0x42A3)
        self.assertEqual(self.header["items"], 23725)
        self.assertEqual(self.header["outfits"], 903)
        self.assertEqual(self.header["effects"], 175)
        self.assertEqual(self.header["missiles"], 54)
        self.assertTrue(self.detection["validated"])
        self.assertEqual(
            self.detection["features"],
            {"extended": True, "improvedAnimations": True, "frameGroups": True},
        )
        self.assertEqual(self.spr.sprite_count, 338944)
        self.assertEqual(len(self.otb.items), 26282)

    def test_client_930(self):
        item = self.entries({("item", 930)})[("item", 930)]
        self.assertTrue(item.flags["isGround"])
        self.assertTrue(item.flags["isFullGround"])
        self.assertTrue(item.flags["isUnmoveable"])
        self.assertTrue(item.flags["miniMap"])
        self.assertEqual(item.values["miniMapColor"], 129)
        self.assertEqual(item.frame_groups[0]["spriteIds"], [5255, 5256])

        matches = [entry for entry in self.otb.items if entry.client_id == 930]
        self.assertEqual([entry.server_id for entry in matches], [784])
        self.assertEqual(matches[0].group_raw, 1)
        self.assertEqual(matches[0].flags_raw, 0x02000000)

    def test_server_930(self):
        matches = [entry for entry in self.otb.items if entry.server_id == 930]
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0].client_id, 1162)
        thing = self.entries({("item", 1162)})[("item", 1162)]
        self.assertTrue(thing.flags["isUnpassable"])
        self.assertTrue(thing.flags["isUnmoveable"])
        self.assertEqual(thing.frame_groups[0]["spriteIds"], [9833, 9834])

    def test_outfit_and_missile_parse(self):
        things = self.entries({("outfit", 2), ("missile", 1)})
        outfit = things[("outfit", 2)]
        self.assertEqual(len(outfit.frame_groups), 2)
        self.assertEqual(outfit.frame_groups[1]["type"], 1)
        self.assertEqual(outfit.frame_groups[1]["frames"], 2)
        missile = things[("missile", 1)]
        self.assertEqual(
            len({sid for sid in missile.frame_groups[0]["spriteIds"] if sid}), 12
        )

    def test_sprite_hash_oracle(self):
        """Recompute OTB sprite hashes from DAT + SPR the ObjectBuilder way."""
        def thing_hash(thing):
            group = thing.frame_groups[0]
            count = group["width"] * group["height"] * group["layers"]
            buffer = bytearray()
            for index in range(count):
                sprite_id = group["spriteIds"][index]
                compressed = self.spr.read_compressed(sprite_id) if sprite_id else None
                if compressed is None or not compressed[0]:
                    buffer += bytes([0x11, 0x11, 0x11, 0]) * 1024
                    continue
                pixels, _, _ = self.spr.decode(compressed[0], transparent=False)
                for y in range(32):
                    row = (31 - y) * 32 * 4
                    for x in range(32):
                        r, g, b, a = pixels[row + x * 4 : row + x * 4 + 4]
                        if a == 0:
                            r = g = b = 0x11
                        buffer += bytes([b, g, r, 0])
            return hashlib.md5(bytes(buffer)).digest().hex()

        hashes = {
            entry.client_id: entry.attributes["spriteHash"]
            for entry in self.otb.items
            if entry.client_id and entry.attributes.get("spriteHash")
        }
        sample = [100, 930, 5000, 15000, 23626 + 99]
        things = self.entries({("item", client): None for client in sample})
        for client in sample:
            self.assertEqual(thing_hash(things[("item", client)]), hashes[client],
                             "hash mismatch for client id %d" % client)

    def test_cli_smoke(self):
        code, document = run_cli(
            [
                "--dat", TEST_CLIENT_DIR / "Tibia.dat",
                "--spr", TEST_CLIENT_DIR / "Tibia.spr",
                "--otb", TEST_CLIENT_DIR / "items.otb",
                "client", "930",
            ]
        )
        self.assertEqual(code, 0)
        self.assertEqual(document["gameVersion"]["label"], "10.98")
        self.assertEqual(document["counts"]["items"], 23626)
        self.assertEqual(document["counts"]["maxItemId"], 23725)
        self.assertEqual(document["result"]["otbMapping"]["serverIds"], [784])
        self.assertFalse(document["features"]["transparency"])
        self.assertEqual(
            document["features"]["detection"]["transparency"]["mode"], "rgb"
        )

    def test_cli_dump_sprites(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, document = run_cli(
                [
                    "--dat", TEST_CLIENT_DIR / "Tibia.dat",
                    "--spr", TEST_CLIENT_DIR / "Tibia.spr",
                    "client", "930", "--dump-sprites", tmp, "--sheet",
                ]
            )
            self.assertEqual(code, 0)
            export = document["result"]["spriteExport"]
            self.assertEqual(export["sheetOrder"], [5255, 5256])
            for entry in export["sprites"]:
                width, height, rows = png_pixels(Path(entry["file"]).read_bytes())
                self.assertEqual((width, height), (32, 32))
            width, height, _ = png_pixels(Path(export["sheet"]).read_bytes())
            self.assertEqual((width, height), (64, 32))


if __name__ == "__main__":
    unittest.main(verbosity=2)
