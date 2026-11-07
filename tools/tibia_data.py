#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""tibia_data.py - Self-contained Tibia client data inspector (DAT / SPR / OTB).

Read-only CLI for AI agents and developers that parses Tibia client files:
  * DAT  - thing metadata   (items, outfits, effects, missiles)
  * SPR  - sprite images    (32x32 sprite blobs + PNG export)
  * OTB  - server item data (items.otb: server id <-> client id, flags, attrs)

Client ids follow the ObjectBuilder convention (ThingTypeStorage):
items are numbered from 100 (the DAT header stores the max item id), while
outfits, effects and missiles are numbered from 1.

The tool auto-detects the client version (7.10 ... 13.10+) from the DAT/SPR
signatures, auto-detects the format feature set (extended, transparency,
improved animations, frame groups) by validating a full parse, and prints
JSON to stdout.  Every invocation is a single shot: it parses what it needs
and exits with a JSON document.

Format knowledge mirrors the ObjectBuilder implementation
(src/otlib/things/MetadataReader*.as, src/otlib/sprites/SpriteReader.as,
src/otlib/items/OtbReader.as).

Examples
--------
  python tibia_data.py --dat Tibia.dat --spr Tibia.spr --otb items.otb info
  python tibia_data.py --dat Tibia.dat --spr Tibia.spr --otb items.otb client 930
  python tibia_data.py --dat Tibia.dat --spr Tibia.spr --otb items.otb server 930
  python tibia_data.py --dat Tibia.dat --spr Tibia.spr client 12 --category outfit
  python tibia_data.py --dat Tibia.dat --spr Tibia.spr --otb items.otb client 930 --dump-sprites out/
  python tibia_data.py --spr Tibia.spr sprite 1234 --out out/

Exit codes: 0 = success, 1 = error (errors are reported as JSON too).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import mmap
import struct
import sys
import zlib
from pathlib import Path

TOOL_NAME = "tibia_data.py"
DEFAULT_SPRITE_SIZE = 32
MAX_THING_SPRITES = 4096  # ObjectBuilder SpriteExtent.DEFAULT_DATA_SIZE

# Frame durations used when the client stores no per-frame durations
# (ObjectBuilder ObjectBuilderSettings defaults).
DEFAULT_DURATION = {
    "item": 500,
    "outfit": 300,
    "effect": 100,
    "missile": 75,
}

CATEGORIES = ("item", "outfit", "effect", "missile")

# ObjectBuilder ThingTypeStorage: item ids start at 100, all other categories
# start at 1.  The DAT header stores the MAX item id, so the number of item
# entries is (header.items - MIN_ITEM_ID + 1).
MIN_ITEM_ID = 100
MIN_THING_IDS = {"item": MIN_ITEM_ID, "outfit": 1, "effect": 1, "missile": 1}


def dat_thing_range(header):
    """Return {category: (first_id, last_id)} from a DAT header."""
    return {
        "item": (MIN_ITEM_ID, header["items"]),
        "outfit": (1, header["outfits"]),
        "effect": (1, header["effects"]),
        "missile": (1, header["missiles"]),
    }

# --------------------------------------------------------------------------
# Version table (src/config/versions.xml)
# (value, label, dat signature, spr signature, otb version)
# --------------------------------------------------------------------------

VERSIONS = [
    (710, "7.10", 0x3DFF4B2A, 0x3DFF4AEB, 0),
    (730, "7.30", 0x411A6233, 0x411A6279, 0),
    (740, "7.40", 0x41BF619C, 0x41B9EA86, 1),
    (750, "7.50", 0x42F81973, 0x42F81949, 1),
    (755, "7.55", 0x437B2B8F, 0x434F9CDE, 2),
    (760, "7.60", 0x439D5A33, 0x439852BE, 3),
    (770, "7.70", 0x439D5A33, 0x439852BE, 3),
    (780, "7.80", 0x44CE4743, 0x44CE4206, 4),
    (790, "7.90", 0x457D854E, 0x457957C8, 5),
    (792, "7.92", 0x459E7B73, 0x45880FE8, 6),
    (800, "8.00", 0x467FD7E6, 0x467F9E74, 7),
    (810, "8.10", 0x475D3747, 0x475D0B01, 8),
    (811, "8.11", 0x47F60E37, 0x47EBB9B2, 9),
    (820, "8.20", 0x486905AA, 0x4868ECC9, 10),
    (830, "8.30", 0x48DA1FB6, 0x48C8E712, 11),
    (840, "8.40", 0x493D607A, 0x493D4E7C, 12),
    (841, "8.41", 0x49B7CC19, 0x49B140EA, 13),
    (842, "8.42", 0x49C233C9, 0x49B140EA, 14),
    (850, "8.50 v1", 0x4A49C5EB, 0x4A44FD4E, 15),
    (850, "8.50 v2", 0x4A4CC0DC, 0x4A44FD4E, 15),
    (850, "8.50 v3", 0x4AE97492, 0x4ACB5230, 15),
    (852, "8.52", 0x4A4CC0DC, 0x4A44FD4E, 0),
    (853, "8.53", 0x4AE97492, 0x4ACB5230, 0),
    (854, "8.54 v1", 0x4B1E2CAA, 0x4B1E2C87, 16),
    (854, "8.54 v2", 0x4B0D46A9, 0x4B0D3AFF, 16),
    (854, "8.54 v3", 0x4B28B89E, 0x4B1E2C87, 17),
    (855, "8.55", 0x4B98FF53, 0x4B913871, 18),
    (860, "8.60 v1", 0x4C28B721, 0x4C220594, 19),
    (860, "8.60 v2", 0x4C2C7993, 0x4C220594, 20),
    (861, "8.61", 0x4C6A4CBC, 0x4C63F145, 21),
    (862, "8.62", 0x4C973450, 0x4C63F145, 22),
    (870, "8.70", 0x4CFE22C5, 0x4CFD078A, 23),
    (871, "8.71", 0x4D41979E, 0x4D3D65D0, 24),
    (872, "8.72", 0x4DAD1A1A, 0x4DAD1A32, 25),
    (900, "9.00", 0x4DBAA20B, 0x4DAD1A32, 27),
    (910, "9.10", 0x4E12DAFF, 0x4E12DB27, 28),
    (920, "9.20", 0x4E807C08, 0x4E807C23, 29),
    (940, "9.40", 0x4EE71DE5, 0x4EE71E06, 30),
    (944, "9.44 v0", 0x4F0EEFBB, 0x4F0EEFEF, 31),
    (944, "9.44 v1", 0x4F105168, 0x4F1051D7, 32),
    (944, "9.44 v2", 0x4F16C0D7, 0x4F1051D7, 33),
    (944, "9.44 v3", 0x4F3131CF, 0x4F3131F6, 34),
    (946, "9.46", 0x4F75B7AB, 0x4F5DCEF7, 35),
    (950, "9.50", 0x4F75B7AB, 0x4F75B7CD, 36),
    (952, "9.52", 0x4F857F6C, 0x4F857F8E, 37),
    (953, "9.53", 0x4FA11252, 0x4FA11282, 38),
    (954, "9.54", 0x4FD5956B, 0x4FD595B7, 39),
    (960, "9.60", 0x4FFA74CC, 0x4FFA74F9, 40),
    (961, "9.61", 0x50226F9D, 0x50226FBD, 41),
    (963, "9.63", 0x503CB933, 0x503CB954, 42),
    (970, "9.70", 0x5072A490, 0x5072A567, 43),
    (980, "9.80", 0x50C70674, 0x50C70753, 44),
    (981, "9.81", 0x50D1C5B6, 0x50D1C685, 45),
    (982, "9.82", 0x512CAD09, 0x512CAD68, 46),
    (983, "9.83", 0x51407B67, 0x51407BC7, 47),
    (985, "9.85", 0x51641A1B, 0x51641A84, 48),
    (986, "9.86", 0x5170E904, 0x5170E96F, 49),
    (1010, "10.10", 0x51E3F8C3, 0x51E3F8E9, 50),
    (1020, "10.20", 0x5236F129, 0x5236F14F, 51),
    (1021, "10.21", 0x526A5068, 0x526A5090, 52),
    (1030, "10.30", 0x52A59036, 0x52A5905F, 53),
    (1031, "10.31", 0x52AED581, 0x52AED5A7, 54),
    (1032, "10.32", 0x52D8D0A9, 0x52D8D0CE, 0),
    (1034, "10.34", 0x52E74AB5, 0x52E74ADA, 0),
    (1035, "10.35", 0x52FDFC2C, 0x52FDFC54, 55),
    (1036, "10.36", 0x53159C7E, 0x53159CA9, 0),
    (1037, "10.37", 0x531EA82E, 0x531EA856, 0),
    (1038, "10.38", 0x5333C199, 0x5333C1C3, 0),
    (1039, "10.39", 0x535A50AD, 0x535A50D5, 0),
    (1040, "10.40", 0x5379984D, 0x53799876, 0),
    (1041, "10.41", 0x5383504E, 0x53835077, 0),
    (1050, "10.50", 0x53B6460E, 0x53B64639, 0),
    (1051, "10.51", 0x53C8CC17, 0x53C8CC3F, 0),
    (1052, "10.52", 0x53E898BD, 0x53E898E5, 0),
    (1053, "10.53", 0x53FAD76E, 0x53FAD799, 0),
    (1054, "10.54", 0x540D3A47, 0x53E898E5, 0),
    (1055, "10.55", 0x54128727, 0x54128755, 0),
    (1056, "10.56", 0x542143B0, 0x542143DE, 0),
    (1057, "10.57", 0x542535F9, 0x54253627, 0),
    (1058, "10.58", 0x542D12E7, 0x542D1315, 0),
    (1059, "10.59", 0x5434084B, 0x54340879, 0),
    (1060, "10.60", 0x5448D9C7, 0x5448DA10, 0),
    (1061, "10.61", 0x5448D9C7, 0x5448DA10, 0),
    (1062, "10.62", 0x54622638, 0x54622667, 0),
    (1063, "10.63", 0x546B502A, 0x546B505E, 0),
    (1064, "10.64", 0x547F05BE, 0x547F0632, 0),
    (1070, "10.70", 0x5481BB97, 0x5481BC06, 0),
    (1071, "10.71", 0x334F, 0x548E9EFE, 0),
    (1072, "10.72", 0x3729, 0x54B37B99, 0),
    (1073, "10.73", 0x374D, 0x54BC95AE, 0),
    (1074, "10.74", 0x375E, 0x54C5FAB2, 0),
    (1075, "10.75", 0x3775, 0x54D85085, 0),
    (1076, "10.76", 0x37DF, 0x54F03CE9, 0),
    (1077, "10.77", 0x38DE, 0x5525213D, 0),
    (1090, "10.90", 0x3F26, 0x565EE171, 0),
    (1091, "10.91", 0x3F81, 0x56BC8198, 0),
    (1092, "10.92", 0x4086, 0x570742B8, 0),
    (1093, "10.93 test", 0x40FF, 0x57161DEA, 0),
    (1093, "10.93", 0x413F, 0x5726E657, 0),
    (1094, "10.94", 0x41E5, 0x57459D43, 0),
    (1095, "10.95", 0x41F3, 0x575A84BD, 0),
    (1098, "10.98", 0x42A3, 0x57BBD603, 0),
    (1099, "10.99", 0x4347, 0x57FF106B, 0),
    (1310, "13.10", 0x4A10, 0x59E48E02, 0),
]


class ParseError(Exception):
    """Raised when a file does not match the expected binary structure."""


def find_versions(dat_sig=None, spr_sig=None):
    """Return all version rows matching the given signature(s), best first."""
    matches = []
    for row in VERSIONS:
        if dat_sig is not None and row[2] != dat_sig:
            continue
        if spr_sig is not None and row[3] != spr_sig:
            continue
        matches.append(row)
    return matches


def find_version_by_label(label):
    for row in VERSIONS:
        if row[1] == label:
            return row
    return None


def format_sig(value):
    return "0x%X" % value


def pick_reader_kind(version_value):
    """MetadataReader selection (MetadataControllerStorage.createReader)."""
    if version_value <= 730:
        return 1
    if version_value <= 750:
        return 2
    if version_value <= 772:
        return 3
    if version_value <= 854:
        return 4
    if version_value <= 986:
        return 5
    return 6


# --------------------------------------------------------------------------
# Little-endian binary reader
# --------------------------------------------------------------------------


class BinReader:
    """Bounds-checked little-endian reader."""

    __slots__ = ("data", "pos", "size")

    def __init__(self, data, pos=0):
        self.data = data
        self.pos = pos
        self.size = len(data)

    def need(self, count):
        if self.pos + count > self.size:
            raise ParseError(
                "unexpected end of file at offset %d (need %d byte(s), %d left)"
                % (self.pos, count, self.size - self.pos)
            )

    def u8(self):
        self.need(1)
        value = self.data[self.pos]
        self.pos += 1
        return value

    def i8(self):
        value = self.u8()
        return value - 256 if value > 127 else value

    def u16(self):
        self.need(2)
        data = self.data
        pos = self.pos
        self.pos = pos + 2
        return data[pos] | (data[pos + 1] << 8)

    def i16(self):
        value = self.u16()
        return value - 0x10000 if value > 0x7FFF else value

    def u32(self):
        self.need(4)
        data = self.data
        pos = self.pos
        self.pos = pos + 4
        return (
            data[pos]
            | (data[pos + 1] << 8)
            | (data[pos + 2] << 16)
            | (data[pos + 3] << 24)
        )

    def i32(self):
        value = self.u32()
        return value - 0x100000000 if value > 0x7FFFFFFF else value

    def read(self, count):
        self.need(count)
        value = self.data[self.pos : self.pos + count]
        self.pos += count
        return value


# --------------------------------------------------------------------------
# DAT thing flags
#
# Each flag entry:  code -> (name, bool properties to set, read operations)
# Read operations:  ("u8"|"u16"|"i16"|"u32"|"i32"|"i8", prop),
#                   ("const", prop, value), ("market",) or ("bones",)
# --------------------------------------------------------------------------

# Reader 1: 7.10 - 7.30
FLAGS1 = {
    0x00: ("GROUND", ("isGround",), (("u16", "groundSpeed"),)),
    0x01: ("ON_BOTTOM", ("isOnBottom",), ()),
    0x02: ("ON_TOP", ("isOnTop",), ()),
    0x03: ("CONTAINER", ("isContainer",), ()),
    0x04: ("STACKABLE", ("stackable",), ()),
    0x05: ("MULTI_USE", ("multiUse",), ()),
    0x06: ("FORCE_USE", ("forceUse",), ()),
    0x07: ("WRITABLE", ("writable",), (("u16", "maxReadWriteChars"),)),
    0x08: ("WRITABLE_ONCE", ("writableOnce",), (("u16", "maxReadChars"),)),
    0x09: ("FLUID_CONTAINER", ("isFluidContainer",), ()),
    0x0A: ("FLUID", ("isFluid",), ()),
    0x0B: ("UNPASSABLE", ("isUnpassable",), ()),
    0x0C: ("UNMOVEABLE", ("isUnmoveable",), ()),
    0x0D: ("BLOCK_MISSILE", ("blockMissile",), ()),
    0x0E: ("BLOCK_PATHFINDER", ("blockPathfind",), ()),
    0x0F: ("PICKUPABLE", ("pickupable",), ()),
    0x10: ("HAS_LIGHT", ("hasLight",), (("u16", "lightLevel"), ("u16", "lightColor"))),
    0x11: ("FLOOR_CHANGE", ("floorChange",), ()),
    0x12: ("FULL_GROUND", ("isFullGround",), ()),
    0x13: ("HAS_ELEVATION", ("hasElevation",), (("u16", "elevation"),)),
    0x14: ("HAS_OFFSET", ("hasOffset",), (("const", "offsetX", 8), ("const", "offsetY", 8))),
    0x16: ("MINI_MAP", ("miniMap",), (("u16", "miniMapColor"),)),
    0x17: ("ROTATABLE", ("rotatable",), ()),
    0x18: ("LYING_OBJECT", ("isLyingObject",), ()),
    0x19: ("ANIMATE_ALWAYS", ("animateAlways",), ()),
    0x1A: ("LENS_HELP", ("isLensHelp",), (("u16", "lensHelp"),)),
    0x24: ("WRAPPABLE", ("wrappable",), ()),
    0x25: ("UNWRAPPABLE", ("unwrappable",), ()),
    0x26: ("TOP_EFFECT", ("topEffect",), ()),
}

# Reader 2: 7.40 - 7.50
FLAGS2 = {
    0x00: ("GROUND", ("isGround",), (("u16", "groundSpeed"),)),
    0x01: ("ON_BOTTOM", ("isOnBottom",), ()),
    0x02: ("ON_TOP", ("isOnTop",), ()),
    0x03: ("CONTAINER", ("isContainer",), ()),
    0x04: ("STACKABLE", ("stackable",), ()),
    0x05: ("MULTI_USE", ("multiUse",), ()),
    0x06: ("FORCE_USE", ("forceUse",), ()),
    0x07: ("WRITABLE", ("writable",), (("u16", "maxReadWriteChars"),)),
    0x08: ("WRITABLE_ONCE", ("writableOnce",), (("u16", "maxReadChars"),)),
    0x09: ("FLUID_CONTAINER", ("isFluidContainer",), ()),
    0x0A: ("FLUID", ("isFluid",), ()),
    0x0B: ("UNPASSABLE", ("isUnpassable",), ()),
    0x0C: ("UNMOVEABLE", ("isUnmoveable",), ()),
    0x0D: ("BLOCK_MISSILE", ("blockMissile",), ()),
    0x0E: ("BLOCK_PATHFINDER", ("blockPathfind",), ()),
    0x0F: ("PICKUPABLE", ("pickupable",), ()),
    0x10: ("HAS_LIGHT", ("hasLight",), (("u16", "lightLevel"), ("u16", "lightColor"))),
    0x11: ("FLOOR_CHANGE", ("floorChange",), ()),
    0x12: ("FULL_GROUND", ("isFullGround",), ()),
    0x13: ("HAS_ELEVATION", ("hasElevation",), (("u16", "elevation"),)),
    0x14: ("HAS_OFFSET", ("hasOffset",), (("const", "offsetX", 8), ("const", "offsetY", 8))),
    0x16: ("MINI_MAP", ("miniMap",), (("u16", "miniMapColor"),)),
    0x17: ("ROTATABLE", ("rotatable",), ()),
    0x18: ("LYING_OBJECT", ("isLyingObject",), ()),
    0x19: ("HANGABLE", ("hangable",), ()),
    0x1A: ("VERTICAL", ("isVertical",), ()),
    0x1B: ("HORIZONTAL", ("isHorizontal",), ()),
    0x1C: ("ANIMATE_ALWAYS", ("animateAlways",), ()),
    0x1D: ("LENS_HELP", ("isLensHelp",), (("u16", "lensHelp"),)),
    0x24: ("WRAPPABLE", ("wrappable",), ()),
    0x25: ("UNWRAPPABLE", ("unwrappable",), ()),
    0x26: ("TOP_EFFECT", ("topEffect",), ()),
}

# Reader 3: 7.55 - 7.72
FLAGS3 = {
    0x00: ("GROUND", ("isGround",), (("u16", "groundSpeed"),)),
    0x01: ("GROUND_BORDER", ("isGroundBorder",), ()),
    0x02: ("ON_BOTTOM", ("isOnBottom",), ()),
    0x03: ("ON_TOP", ("isOnTop",), ()),
    0x04: ("CONTAINER", ("isContainer",), ()),
    0x05: ("STACKABLE", ("stackable",), ()),
    0x06: ("FORCE_USE", ("forceUse",), ()),
    0x07: ("MULTI_USE", ("multiUse",), ()),
    0x08: ("WRITABLE", ("writable",), (("u16", "maxReadWriteChars"),)),
    0x09: ("WRITABLE_ONCE", ("writableOnce",), (("u16", "maxReadChars"),)),
    0x0A: ("FLUID_CONTAINER", ("isFluidContainer",), ()),
    0x0B: ("FLUID", ("isFluid",), ()),
    0x0C: ("UNPASSABLE", ("isUnpassable",), ()),
    0x0D: ("UNMOVEABLE", ("isUnmoveable",), ()),
    0x0E: ("BLOCK_MISSILE", ("blockMissile",), ()),
    0x0F: ("BLOCK_PATHFINDER", ("blockPathfind",), ()),
    0x10: ("PICKUPABLE", ("pickupable",), ()),
    0x11: ("HANGABLE", ("hangable",), ()),
    0x12: ("VERTICAL", ("isVertical",), ()),
    0x13: ("HORIZONTAL", ("isHorizontal",), ()),
    0x14: ("ROTATABLE", ("rotatable",), ()),
    0x15: ("HAS_LIGHT", ("hasLight",), (("u16", "lightLevel"), ("u16", "lightColor"))),
    0x17: ("FLOOR_CHANGE", ("floorChange",), ()),
    0x18: ("HAS_OFFSET", ("hasOffset",), (("i16", "offsetX"), ("i16", "offsetY"))),
    0x19: ("HAS_ELEVATION", ("hasElevation",), (("u16", "elevation"),)),
    0x1A: ("LYING_OBJECT", ("isLyingObject",), ()),
    0x1B: ("ANIMATE_ALWAYS", ("animateAlways",), ()),
    0x1C: ("MINI_MAP", ("miniMap",), (("u16", "miniMapColor"),)),
    0x1D: ("LENS_HELP", ("isLensHelp",), (("u16", "lensHelp"),)),
    0x1E: ("FULL_GROUND", ("isFullGround",), ()),
}

# Reader 4: 7.80 - 8.54
FLAGS4 = {
    0x00: ("GROUND", ("isGround",), (("u16", "groundSpeed"),)),
    0x01: ("GROUND_BORDER", ("isGroundBorder",), ()),
    0x02: ("ON_BOTTOM", ("isOnBottom",), ()),
    0x03: ("ON_TOP", ("isOnTop",), ()),
    0x04: ("CONTAINER", ("isContainer",), ()),
    0x05: ("STACKABLE", ("stackable",), ()),
    0x06: ("FORCE_USE", ("forceUse",), ()),
    0x07: ("MULTI_USE", ("multiUse",), ()),
    0x08: ("HAS_CHARGES", ("hasCharges",), ()),
    0x09: ("WRITABLE", ("writable",), (("u16", "maxReadWriteChars"),)),
    0x0A: ("WRITABLE_ONCE", ("writableOnce",), (("u16", "maxReadChars"),)),
    0x0B: ("FLUID_CONTAINER", ("isFluidContainer",), ()),
    0x0C: ("FLUID", ("isFluid",), ()),
    0x0D: ("UNPASSABLE", ("isUnpassable",), ()),
    0x0E: ("UNMOVEABLE", ("isUnmoveable",), ()),
    0x0F: ("BLOCK_MISSILE", ("blockMissile",), ()),
    0x10: ("BLOCK_PATHFIND", ("blockPathfind",), ()),
    0x11: ("PICKUPABLE", ("pickupable",), ()),
    0x12: ("HANGABLE", ("hangable",), ()),
    0x13: ("VERTICAL", ("isVertical",), ()),
    0x14: ("HORIZONTAL", ("isHorizontal",), ()),
    0x15: ("ROTATABLE", ("rotatable",), ()),
    0x16: ("HAS_LIGHT", ("hasLight",), (("u16", "lightLevel"), ("u16", "lightColor"))),
    0x17: ("DONT_HIDE", ("dontHide",), ()),
    0x18: ("FLOOR_CHANGE", ("floorChange",), ()),
    0x19: ("HAS_OFFSET", ("hasOffset",), (("i16", "offsetX"), ("i16", "offsetY"))),
    0x1A: ("HAS_ELEVATION", ("hasElevation",), (("u16", "elevation"),)),
    0x1B: ("LYING_OBJECT", ("isLyingObject",), ()),
    0x1C: ("ANIMATE_ALWAYS", ("animateAlways",), ()),
    0x1D: ("MINI_MAP", ("miniMap",), (("u16", "miniMapColor"),)),
    0x1E: ("LENS_HELP", ("isLensHelp",), (("u16", "lensHelp"),)),
    0x1F: ("FULL_GROUND", ("isFullGround",), ()),
    0x20: ("IGNORE_LOOK", ("ignoreLook",), ()),
    0x24: ("WRAPPABLE", ("wrappable",), ()),
    0x25: ("UNWRAPPABLE", ("unwrappable",), ()),
    0x27: ("HAS_BONES", ("hasBones",), (("bones",),)),
}

# Reader 5: 8.60 - 9.86
FLAGS5 = {
    0x00: ("GROUND", ("isGround",), (("u16", "groundSpeed"),)),
    0x01: ("GROUND_BORDER", ("isGroundBorder",), ()),
    0x02: ("ON_BOTTOM", ("isOnBottom",), ()),
    0x03: ("ON_TOP", ("isOnTop",), ()),
    0x04: ("CONTAINER", ("isContainer",), ()),
    0x05: ("STACKABLE", ("stackable",), ()),
    0x06: ("FORCE_USE", ("forceUse",), ()),
    0x07: ("MULTI_USE", ("multiUse",), ()),
    0x08: ("WRITABLE", ("writable",), (("u16", "maxReadWriteChars"),)),
    0x09: ("WRITABLE_ONCE", ("writableOnce",), (("u16", "maxReadChars"),)),
    0x0A: ("FLUID_CONTAINER", ("isFluidContainer",), ()),
    0x0B: ("FLUID", ("isFluid",), ()),
    0x0C: ("UNPASSABLE", ("isUnpassable",), ()),
    0x0D: ("UNMOVEABLE", ("isUnmoveable",), ()),
    0x0E: ("BLOCK_MISSILE", ("blockMissile",), ()),
    0x0F: ("BLOCK_PATHFIND", ("blockPathfind",), ()),
    0x10: ("PICKUPABLE", ("pickupable",), ()),
    0x11: ("HANGABLE", ("hangable",), ()),
    0x12: ("VERTICAL", ("isVertical",), ()),
    0x13: ("HORIZONTAL", ("isHorizontal",), ()),
    0x14: ("ROTATABLE", ("rotatable",), ()),
    0x15: ("HAS_LIGHT", ("hasLight",), (("u16", "lightLevel"), ("u16", "lightColor"))),
    0x16: ("DONT_HIDE", ("dontHide",), ()),
    0x17: ("TRANSLUCENT", ("isTranslucent",), ()),
    0x18: ("HAS_OFFSET", ("hasOffset",), (("i16", "offsetX"), ("i16", "offsetY"))),
    0x19: ("HAS_ELEVATION", ("hasElevation",), (("u16", "elevation"),)),
    0x1A: ("LYING_OBJECT", ("isLyingObject",), ()),
    0x1B: ("ANIMATE_ALWAYS", ("animateAlways",), ()),
    0x1C: ("MINI_MAP", ("miniMap",), (("u16", "miniMapColor"),)),
    0x1D: ("LENS_HELP", ("isLensHelp",), (("u16", "lensHelp"),)),
    0x1E: ("FULL_GROUND", ("isFullGround",), ()),
    0x1F: ("IGNORE_LOOK", ("ignoreLook",), ()),
    0x20: ("CLOTH", ("cloth",), (("u16", "clothSlot"),)),
    0x21: ("MARKET_ITEM", ("isMarketItem",), (("market",),)),
    0x27: ("HAS_BONES", ("hasBones",), (("bones",),)),
}

# Reader 6: 10.10+
FLAGS6 = {
    0x00: ("GROUND", ("isGround",), (("u16", "groundSpeed"),)),
    0x01: ("GROUND_BORDER", ("isGroundBorder",), ()),
    0x02: ("ON_BOTTOM", ("isOnBottom",), ()),
    0x03: ("ON_TOP", ("isOnTop",), ()),
    0x04: ("CONTAINER", ("isContainer",), ()),
    0x05: ("STACKABLE", ("stackable",), ()),
    0x06: ("FORCE_USE", ("forceUse",), ()),
    0x07: ("MULTI_USE", ("multiUse",), ()),
    0x08: ("WRITABLE", ("writable",), (("u16", "maxReadWriteChars"),)),
    0x09: ("WRITABLE_ONCE", ("writableOnce",), (("u16", "maxReadChars"),)),
    0x0A: ("FLUID_CONTAINER", ("isFluidContainer",), ()),
    0x0B: ("FLUID", ("isFluid",), ()),
    0x0C: ("UNPASSABLE", ("isUnpassable",), ()),
    0x0D: ("UNMOVEABLE", ("isUnmoveable",), ()),
    0x0E: ("BLOCK_MISSILE", ("blockMissile",), ()),
    0x0F: ("BLOCK_PATHFIND", ("blockPathfind",), ()),
    0x10: ("NO_MOVE_ANIMATION", ("noMoveAnimation",), ()),
    0x11: ("PICKUPABLE", ("pickupable",), ()),
    0x12: ("HANGABLE", ("hangable",), ()),
    0x13: ("VERTICAL", ("isVertical",), ()),
    0x14: ("HORIZONTAL", ("isHorizontal",), ()),
    0x15: ("ROTATABLE", ("rotatable",), ()),
    0x16: ("HAS_LIGHT", ("hasLight",), (("u16", "lightLevel"), ("u16", "lightColor"))),
    0x17: ("DONT_HIDE", ("dontHide",), ()),
    0x18: ("TRANSLUCENT", ("isTranslucent",), ()),
    0x19: ("HAS_OFFSET", ("hasOffset",), (("i16", "offsetX"), ("i16", "offsetY"))),
    0x1A: ("HAS_ELEVATION", ("hasElevation",), (("u16", "elevation"),)),
    0x1B: ("LYING_OBJECT", ("isLyingObject",), ()),
    0x1C: ("ANIMATE_ALWAYS", ("animateAlways",), ()),
    0x1D: ("MINI_MAP", ("miniMap",), (("u16", "miniMapColor"),)),
    0x1E: ("LENS_HELP", ("isLensHelp",), (("u16", "lensHelp"),)),
    0x1F: ("FULL_GROUND", ("isFullGround",), ()),
    0x20: ("IGNORE_LOOK", ("ignoreLook",), ()),
    0x21: ("CLOTH", ("cloth",), (("u16", "clothSlot"),)),
    0x22: ("MARKET_ITEM", ("isMarketItem",), (("market",),)),
    0x23: ("DEFAULT_ACTION", ("hasDefaultAction",), (("u16", "defaultAction"),)),
    0x24: ("WRAPPABLE", ("wrappable",), ()),
    0x25: ("UNWRAPPABLE", ("unwrappable",), ()),
    0x26: ("TOP_EFFECT", ("topEffect",), ()),
    0x27: ("HAS_BONES", ("hasBones",), (("bones",),)),
    0xFE: ("USABLE", ("usable",), ()),
}

FLAGS_BY_READER = {1: FLAGS1, 2: FLAGS2, 3: FLAGS3, 4: FLAGS4, 5: FLAGS5, 6: FLAGS6}

# Directions order used by HAS_BONES (North, South, East, West)
BONES_DIRECTIONS = ("north", "south", "east", "west")

# Every boolean flag a ThingType can have (false defaults are emitted too).
ALL_THING_FLAGS = (
    "isGround",
    "isGroundBorder",
    "isOnBottom",
    "isOnTop",
    "isContainer",
    "stackable",
    "forceUse",
    "multiUse",
    "hasCharges",
    "writable",
    "writableOnce",
    "isFluidContainer",
    "isFluid",
    "isUnpassable",
    "isUnmoveable",
    "blockMissile",
    "blockPathfind",
    "noMoveAnimation",
    "pickupable",
    "hangable",
    "isVertical",
    "isHorizontal",
    "rotatable",
    "hasLight",
    "dontHide",
    "isTranslucent",
    "floorChange",
    "hasOffset",
    "hasBones",
    "hasElevation",
    "isLyingObject",
    "animateAlways",
    "miniMap",
    "isLensHelp",
    "isFullGround",
    "ignoreLook",
    "cloth",
    "isMarketItem",
    "hasDefaultAction",
    "wrappable",
    "unwrappable",
    "topEffect",
    "usable",
)

ALL_THING_VALUES = (
    "groundSpeed",
    "maxReadWriteChars",
    "maxReadChars",
    "lightLevel",
    "lightColor",
    "offsetX",
    "offsetY",
    "elevation",
    "miniMapColor",
    "lensHelp",
    "clothSlot",
    "marketCategory",
    "marketTradeAs",
    "marketShowAs",
    "marketRestrictProfession",
    "marketRestrictLevel",
    "defaultAction",
)


# --------------------------------------------------------------------------
# DAT parser
# --------------------------------------------------------------------------


class Thing:
    """One parsed DAT thing (item / outfit / effect / missile)."""

    __slots__ = (
        "category",
        "id",
        "flags",
        "values",
        "flag_sequence",
        "frame_groups",
        "market_name",
        "bones_offsets",
    )

    def __init__(self, category, thing_id):
        self.category = category
        self.id = thing_id
        self.flags = {name: False for name in ALL_THING_FLAGS}
        self.values = {name: 0 for name in ALL_THING_VALUES}
        self.flag_sequence = []
        self.frame_groups = []
        self.market_name = None
        self.bones_offsets = None

    def to_json(self):
        data = {
            "category": self.category,
            "id": self.id,
            "flags": dict(self.flags),
            "values": dict(self.values),
            "flagsSet": sorted(k for k, v in self.flags.items() if v),
            "flagSequence": list(self.flag_sequence),
            "frameGroups": [frame_group_to_json(g) for g in self.frame_groups],
            "spriteIds": thing_sprite_ids(self),
        }
        if self.market_name is not None:
            data["marketName"] = self.market_name
        if self.bones_offsets is not None:
            data["bonesOffsets"] = self.bones_offsets
        return data


def frame_group_to_json(group):
    return {
        "type": group["type"],
        "typeName": "walking" if group["type"] == 1 else "default",
        "width": group["width"],
        "height": group["height"],
        "exactSize": group["exactSize"],
        "layers": group["layers"],
        "patternX": group["patternX"],
        "patternY": group["patternY"],
        "patternZ": group["patternZ"],
        "frames": group["frames"],
        "isAnimation": group["isAnimation"],
        "animationMode": group["animationMode"],
        "animationModeName": "synchronous" if group["animationMode"] == 1 else "asynchronous",
        "loopCount": group["loopCount"],
        "startFrame": group["startFrame"],
        "frameDurations": [
            {"min": low, "max": high} for (low, high) in group["frameDurations"]
        ],
        "spriteCount": len(group["spriteIds"]),
        "spriteIds": list(group["spriteIds"]),
    }


def thing_sprite_ids(thing):
    """Unique sprite ids referenced by the thing, in first-seen order."""
    seen = set()
    result = []
    for group in thing.frame_groups:
        for sprite_id in group["spriteIds"]:
            if sprite_id and sprite_id not in seen:
                seen.add(sprite_id)
                result.append(sprite_id)
    return result


class DatFile:
    """Tibia .dat metadata parser (mirrors ObjectBuilder MetadataReader 1-6)."""

    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        if len(self.data) < 12:
            raise ParseError("%s is too small to be a DAT file" % self.path)

    # -- header ------------------------------------------------------------

    def header(self):
        return {
            "datSignature": int.from_bytes(self.data[0:4], "little"),
            "items": int.from_bytes(self.data[4:6], "little"),
            "outfits": int.from_bytes(self.data[6:8], "little"),
            "effects": int.from_bytes(self.data[8:10], "little"),
            "missiles": int.from_bytes(self.data[10:12], "little"),
        }

    # -- feature detection -------------------------------------------------

    def detect(self, version_value, overrides=None, spr_count=None, collect=None):
        """Pick (extended, improvedAnimations, frameGroups) by trial parsing.

        A candidate is accepted only when the whole file parses cleanly to the
        exact end (and all sprite ids fit into the SPR file when known).
        """
        overrides = overrides or {}
        heuristic = {
            "extended": version_value >= 960,
            "improvedAnimations": version_value >= 1010,
            "frameGroups": version_value >= 1010,
        }
        for key, value in overrides.items():
            if value is not None:
                heuristic[key] = value

        header = self.header()
        skip = () if header["outfits"] > 0 else ("frameGroups",)
        candidates = _feature_candidates(heuristic, skip=skip)

        attempts = []
        for combo in candidates:
            try:
                collected, max_sprite_id = self._parse(
                    version_value, combo, collect=collect, spr_count=spr_count
                )
            except ParseError as error:
                attempts.append({"features": combo, "result": str(error)})
                continue
            attempts.append({"features": dict(combo), "result": "ok"})
            return {
                "features": combo,
                "validated": True,
                "reader": "MetadataReader%d" % pick_reader_kind(version_value),
                "attempts": attempts[-4:],
                "collected": collected,
                "maxSpriteId": max_sprite_id,
            }

        return {
            "features": None,
            "validated": False,
            "reader": "MetadataReader%d" % pick_reader_kind(version_value),
            "attempts": attempts,
            "heuristic": heuristic,
            "collected": {},
            "maxSpriteId": None,
        }

    # -- full parse --------------------------------------------------------

    def parse(self, version_value, features, collect=None, spr_count=None):
        """Parse the whole file; return (header, collected, maxSpriteId)."""
        collected, max_sprite_id = self._parse(
            version_value, features, collect=collect, spr_count=spr_count
        )
        return self.header(), collected, max_sprite_id

    def _parse(self, version_value, features, collect=None, spr_count=None):
        reader_kind = pick_reader_kind(version_value)
        reader = BinReader(self.data, 12)
        header = self.header()
        extended = features["extended"]
        improved = features["improvedAnimations"]
        frame_groups = features["frameGroups"]
        pattern_z_supported = reader_kind >= 3

        max_sprite_id = 0

        for category, (first_id, last_id) in dat_thing_range(header).items():
            for thing_id in range(first_id, last_id + 1):
                thing = Thing(category, thing_id)
                _read_thing_flags(reader, thing, reader_kind)
                _read_texture_patterns(
                    reader,
                    thing,
                    category,
                    extended,
                    improved,
                    frame_groups,
                    pattern_z_supported,
                )
                for group in thing.frame_groups:
                    for sprite_id in group["spriteIds"]:
                        if sprite_id > max_sprite_id:
                            max_sprite_id = sprite_id
                if collect is not None and (category, thing_id) in collect:
                    collect[(category, thing_id)] = thing

        if reader.pos != reader.size:
            raise ParseError(
                "DAT parse ended at offset %d but file is %d bytes (%d trailing)"
                % (reader.pos, reader.size, reader.size - reader.pos)
            )
        if spr_count is not None and max_sprite_id > spr_count:
            raise ParseError(
                "sprite id %d exceeds SPR sprite count %d" % (max_sprite_id, spr_count)
            )
        return collect, max_sprite_id


def _feature_candidates(heuristic, skip=()):
    """All feature combos ordered by distance to the heuristic values."""
    keys = ("extended", "improvedAnimations", "frameGroups")
    variants = []
    for key in keys:
        if key in skip:
            # The flag cannot be validated from this file (e.g. no outfits to
            # exercise frame groups); keep the heuristic value.
            variants.append((heuristic[key],))
        else:
            variants.append((False, True) if heuristic[key] else (True, False))
    combos = []
    for extended in variants[0]:
        for improved in variants[1]:
            for frame_groups in variants[2]:
                combo = {
                    "extended": extended,
                    "improvedAnimations": improved,
                    "frameGroups": frame_groups,
                }
                flips = 0
                for key in keys:
                    if key in skip:
                        continue
                    if combo[key] != heuristic[key]:
                        flips += 1
                combos.append((flips, combo))
    combos.sort(key=lambda item: item[0])
    return [combo for _, combo in combos]


def _read_thing_flags(reader, thing, reader_kind):
    table = FLAGS_BY_READER[reader_kind]
    flags = thing.flags
    values = thing.values
    sequence = thing.flag_sequence

    while True:
        offset = reader.pos
        flag = reader.u8()
        if flag == 0xFF:
            return
        entry = table.get(flag)
        if entry is None:
            raise ParseError(
                "unknown DAT flag 0x%02X in %s id %d at offset %d"
                % (flag, thing.category, thing.id, offset)
            )
        name, bool_props, reads = entry
        sequence.append(name)
        for prop in bool_props:
            flags[prop] = True
        for op in reads:
            kind = op[0]
            if kind == "const":
                values[op[1]] = op[2]
            elif kind == "u8":
                values[op[1]] = reader.u8()
            elif kind == "i8":
                values[op[1]] = reader.i8()
            elif kind == "u16":
                values[op[1]] = reader.u16()
            elif kind == "i16":
                values[op[1]] = reader.i16()
            elif kind == "u32":
                values[op[1]] = reader.u32()
            elif kind == "i32":
                values[op[1]] = reader.i32()
            elif kind == "market":
                values["marketCategory"] = reader.u16()
                values["marketTradeAs"] = reader.u16()
                values["marketShowAs"] = reader.u16()
                name_length = reader.u16()
                raw = reader.read(name_length)
                thing.market_name = raw.decode("latin-1")
                values["marketRestrictProfession"] = reader.u16()
                values["marketRestrictLevel"] = reader.u16()
            elif kind == "bones":
                offsets = {}
                for direction in BONES_DIRECTIONS:
                    x = reader.i16()
                    y = reader.i16()
                    offsets[direction] = {"x": x, "y": y}
                thing.bones_offsets = offsets
            else:  # pragma: no cover
                raise ParseError("internal error: unknown read op %r" % (op,))


def _read_texture_patterns(
    reader, thing, category, extended, improved, frame_groups, pattern_z_supported
):
    group_count = 1
    if frame_groups and category == "outfit":
        group_count = reader.u8()

    for _ in range(group_count):
        group = {
            "type": 0,
            "width": 1,
            "height": 1,
            "exactSize": DEFAULT_SPRITE_SIZE,
            "layers": 1,
            "patternX": 1,
            "patternY": 1,
            "patternZ": 1,
            "frames": 1,
            "isAnimation": False,
            "animationMode": 0,
            "loopCount": 0,
            "startFrame": 0,
            "frameDurations": [],
            "spriteIds": [],
        }

        if frame_groups and category == "outfit":
            group["type"] = reader.u8()

        group["width"] = reader.u8()
        group["height"] = reader.u8()
        if group["width"] > 1 or group["height"] > 1:
            group["exactSize"] = reader.u8()
        else:
            group["exactSize"] = DEFAULT_SPRITE_SIZE

        group["layers"] = reader.u8()
        group["patternX"] = reader.u8()
        group["patternY"] = reader.u8()
        if pattern_z_supported:
            group["patternZ"] = reader.u8()
        group["frames"] = reader.u8()

        if group["frames"] > 1:
            group["isAnimation"] = True
            if improved:
                group["animationMode"] = reader.u8()
                group["loopCount"] = reader.i32()
                group["startFrame"] = reader.i8()
                durations = []
                for _ in range(group["frames"]):
                    low = reader.u32()
                    high = reader.u32()
                    durations.append((low, high))
                group["frameDurations"] = durations
            else:
                default = DEFAULT_DURATION[category]
                group["frameDurations"] = [(default, default)] * group["frames"]

        total = (
            group["width"]
            * group["height"]
            * group["patternX"]
            * group["patternY"]
            * group["patternZ"]
            * group["frames"]
            * group["layers"]
        )
        if total > MAX_THING_SPRITES:
            raise ParseError(
                "%s id %d has too many sprites (%d)" % (category, thing.id, total)
            )

        sprite_ids = []
        if extended:
            for _ in range(total):
                sprite_ids.append(reader.u32())
        else:
            for _ in range(total):
                sprite_ids.append(reader.u16())
        group["spriteIds"] = sprite_ids
        thing.frame_groups.append(group)


# --------------------------------------------------------------------------
# SPR parser
# --------------------------------------------------------------------------

SPR_SIGNATURE_OFFSET = 0
SPR_COUNT_OFFSET = 4
SPR_HEADER_U16 = 6
SPR_HEADER_U32 = 8


class SprFile:
    """Tibia .spr sprite file access (random access via the address table)."""

    def __init__(self, path):
        self.path = Path(path)
        self._fh = open(self.path, "rb")
        self._mm = mmap.mmap(self._fh.fileno(), 0, access=mmap.ACCESS_READ)
        self.size = len(self._mm)
        if self.size < SPR_HEADER_U16:
            raise ParseError("%s is too small to be a SPR file" % self.path)
        self.spr_signature = struct.unpack_from("<I", self._mm, SPR_SIGNATURE_OFFSET)[0]
        self.sprite_count = 0
        self.header_size = SPR_HEADER_U16
        self.extended = False
        self._addresses = ()

    def close(self):
        if not self._mm.closed:
            self._mm.close()
        if not self._fh.closed:
            self._fh.close()

    # -- header ------------------------------------------------------------

    def _count_u32(self):
        return struct.unpack_from("<I", self._mm, SPR_COUNT_OFFSET)[0]

    def _count_u16(self):
        return struct.unpack_from("<H", self._mm, SPR_COUNT_OFFSET)[0]

    def _plausible(self, extended):
        count = self._count_u32() if extended else self._count_u16()
        if count == 0:
            return False
        header = SPR_HEADER_U32 if extended else SPR_HEADER_U16
        table_end = header + count * 4
        if table_end > self.size:
            return False
        # Sample addresses: each non-zero one must point past the table and
        # have room for the 5-byte sprite header.
        step = max(1, count // 16)
        for index in range(0, count, step):
            address = struct.unpack_from("<I", self._mm, header + index * 4)[0]
            if address == 0:
                continue
            if address < table_end or address + 5 > self.size:
                return False
        return True

    def detect_extended(self, version_value):
        """Detect the sprite-count field width (extended => uint32)."""
        preferred = version_value >= 960
        for candidate in (preferred, not preferred):
            if self._plausible(candidate):
                return candidate
        raise ParseError("cannot detect the SPR header layout (extended flag)")

    def init(self, extended):
        """Load the address table once the extended flag is known."""
        self.extended = extended
        self.header_size = SPR_HEADER_U32 if extended else SPR_HEADER_U16
        self.sprite_count = self._count_u32() if extended else self._count_u16()
        if self.sprite_count == 0:
            raise ParseError("SPR sprite count is 0")
        table_end = self.header_size + self.sprite_count * 4
        if table_end > self.size:
            raise ParseError(
                "SPR address table (%d sprites) does not fit in %d bytes"
                % (self.sprite_count, self.size)
            )
        self._addresses = struct.unpack_from(
            "<%dI" % self.sprite_count, self._mm, self.header_size
        )
        return self.sprite_count

    # -- sprites -----------------------------------------------------------

    def address(self, sprite_id):
        if not 1 <= sprite_id <= self.sprite_count:
            raise ParseError(
                "sprite id %d out of range 1..%d" % (sprite_id, self.sprite_count)
            )
        return self._addresses[sprite_id - 1]

    def _next_address(self, sprite_id):
        for index in range(sprite_id, len(self._addresses)):
            candidate = self._addresses[index]
            if candidate:
                return candidate
        return self.size

    def read_compressed(self, sprite_id):
        """Return (compressed_bytes, length, address) or None when address=0."""
        address = self.address(sprite_id)
        if address == 0:
            return None
        if address + 5 > self.size:
            raise ParseError(
                "sprite %d address 0x%X is outside the file" % (sprite_id, address)
            )
        length = struct.unpack_from("<H", self._mm, address + 3)[0]
        start = address + 5
        if start + length > self.size:
            raise ParseError(
                "sprite %d data (%d bytes at 0x%X) exceeds file size"
                % (sprite_id, length, start)
            )
        return bytes(self._mm[start : start + length]), length, address

    def data_gap(self, sprite_id, address, length):
        """Bytes between this sprite's data end and the next sprite start."""
        return self._next_address(sprite_id) - address - 5 - length

    def decode(self, compressed, transparent):
        """Decode compressed pixels into 32x32 RGBA.

        Returns (pixels, visible_pixels, total_pixels).
        """
        channels = 4 if transparent else 3
        pixels = bytearray(DEFAULT_SPRITE_SIZE * DEFAULT_SPRITE_SIZE * 4)
        capacity = DEFAULT_SPRITE_SIZE * DEFAULT_SPRITE_SIZE
        write = 0
        colored_total = 0
        pos = 0
        total = 0
        size = len(compressed)
        while pos < size:
            if pos + 4 > size:
                raise ParseError("truncated sprite chunk header at %d" % pos)
            transparent_pixels = compressed[pos] | (compressed[pos + 1] << 8)
            colored_pixels = compressed[pos + 2] | (compressed[pos + 3] << 8)
            pos += 4
            needed = colored_pixels * channels
            if pos + needed > size:
                raise ParseError(
                    "sprite chunk needs %d bytes but only %d remain"
                    % (needed, size - pos)
                )
            write += transparent_pixels
            for _ in range(colored_pixels):
                if transparent:
                    red, green, blue, alpha = compressed[pos : pos + 4]
                else:
                    red, green, blue = compressed[pos : pos + 3]
                    alpha = 0xFF
                pos += channels
                if write < capacity:
                    offset = write * 4
                    pixels[offset] = red
                    pixels[offset + 1] = green
                    pixels[offset + 2] = blue
                    pixels[offset + 3] = alpha
                write += 1
            colored_total += colored_pixels
            total += transparent_pixels + colored_pixels
        return bytes(pixels), colored_total, total

    def detect_transparency(self, samples=24):
        """Detect per-pixel alpha (4-byte colored pixels) by probing sprites."""
        state = {"rgba": {"ok": True, "count": 0, "failures": []},
                 "rgb": {"ok": True, "count": 0, "failures": []}}
        candidates = self._sample_sprite_ids(samples)
        for sprite_id in candidates:
            try:
                compressed = self.read_compressed(sprite_id)
            except ParseError as error:
                for mode in ("rgba", "rgb"):
                    state[mode]["ok"] = False
                    state[mode]["failures"].append("sprite %d: %s" % (sprite_id, error))
                continue
            if compressed is None:
                continue
            data, length, address = compressed
            for mode, transparent in (("rgba", True), ("rgb", False)):
                try:
                    _, visible, total = self.decode(data, transparent)
                    if total > DEFAULT_SPRITE_SIZE * DEFAULT_SPRITE_SIZE:
                        raise ParseError(
                            "chunks describe %d pixels (max %d)"
                            % (total, DEFAULT_SPRITE_SIZE * DEFAULT_SPRITE_SIZE)
                        )
                    gap = self.data_gap(sprite_id, address, length)
                    if gap != 0:
                        raise ParseError("%d stray bytes before next sprite" % gap)
                    state[mode]["count"] += 1
                except ParseError as error:
                    state[mode]["ok"] = False
                    state[mode]["failures"].append("sprite %d: %s" % (sprite_id, error))

        tested = max(state["rgba"]["count"], state["rgb"]["count"])
        if tested == 0:
            return None, {"mode": None, "samplesTested": 0,
                          "note": "no non-empty sprites found"}
        rgba_ok = state["rgba"]["ok"] and state["rgba"]["count"] == len(candidates)
        rgb_ok = state["rgb"]["ok"] and state["rgb"]["count"] == len(candidates)
        if rgba_ok and not rgb_ok:
            return True, {"mode": "rgba", "confidence": "validated",
                          "samplesTested": len(candidates)}
        if rgb_ok and not rgba_ok:
            return False, {"mode": "rgb", "confidence": "validated",
                           "samplesTested": len(candidates)}
        detail = {
            "mode": None,
            "samplesTested": len(candidates),
            "rgbaValid": rgba_ok,
            "rgbValid": rgb_ok,
            "rgbaFailures": state["rgba"]["failures"][:3],
            "rgbFailures": state["rgb"]["failures"][:3],
        }
        return None, detail

    def _sample_sprite_ids(self, samples):
        if self.sprite_count == 0:
            return []
        result = []
        step = max(1, self.sprite_count // (samples * 4))
        sprite_id = 1
        while sprite_id <= self.sprite_count and len(result) < samples:
            address = self._addresses[sprite_id - 1]
            if address:
                length = struct.unpack_from("<H", self._mm, address + 3)[0]
                if length > 0:
                    result.append(sprite_id)
            sprite_id += step
        return result


# --------------------------------------------------------------------------
# PNG writer (pure stdlib)
# --------------------------------------------------------------------------


def _png_chunk(tag, payload):
    return (
        struct.pack(">I", len(payload))
        + tag
        + payload
        + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
    )


def encode_png(width, height, rgba):
    if len(rgba) != width * height * 4:
        raise ValueError("pixel buffer size mismatch")
    rows = bytearray()
    stride = width * 4
    for y in range(height):
        rows.append(0)  # filter type: None
        rows += rgba[y * stride : (y + 1) * stride]
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", header)
        + _png_chunk(b"IDAT", zlib.compress(bytes(rows), 9))
        + _png_chunk(b"IEND", b"")
    )


def write_sprite_png(path, pixels, size=DEFAULT_SPRITE_SIZE):
    Path(path).write_bytes(encode_png(size, size, pixels))


def compose_sheet(sprite_pixels, columns=8, size=DEFAULT_SPRITE_SIZE):
    count = len(sprite_pixels)
    if count == 0:
        return 0, 0, b""
    columns = max(1, min(columns, count))
    rows = (count + columns - 1) // columns
    width = columns * size
    height = rows * size
    canvas = bytearray(width * height * 4)
    for index, pixels in enumerate(sprite_pixels):
        col = index % columns
        row = index // columns
        for y in range(size):
            src = y * size * 4
            dst = ((row * size + y) * width + col * size) * 4
            canvas[dst : dst + size * 4] = pixels[src : src + size * 4]
    return width, height, bytes(canvas)


# --------------------------------------------------------------------------
# OTB parser (mirrors ObjectBuilder OtbReader)
# --------------------------------------------------------------------------

OTB_NODE_START = 0xFE
OTB_NODE_END = 0xFF
OTB_ESCAPE = 0xFD

OTB_GROUPS = {
    0: "none",
    1: "ground",
    2: "container",
    3: "weapon",
    4: "ammunition",
    5: "armor",
    6: "changes",
    7: "teleport",
    8: "magicField",
    9: "writable",
    10: "key",
    11: "splash",
    12: "fluid",
    13: "door",
    14: "deprecated",
}

# ObjectBuilder ServerItemFlag
OTB_FLAG_BITS = (
    ("UNPASSABLE", 0),
    ("BLOCK_MISSILES", 1),
    ("BLOCK_PATHFINDER", 2),
    ("HAS_ELEVATION", 3),
    ("MULTI_USE", 4),
    ("PICKUPABLE", 5),
    ("MOVABLE", 6),
    ("STACKABLE", 7),
    ("FLOOR_CHANGE_DOWN", 8),
    ("FLOOR_CHANGE_NORTH", 9),
    ("FLOOR_CHANGE_EAST", 10),
    ("FLOOR_CHANGE_SOUTH", 11),
    ("FLOOR_CHANGE_WEST", 12),
    ("STACK_ORDER", 13),
    ("READABLE", 14),
    ("ROTATABLE", 15),
    ("HANGABLE", 16),
    ("HOOK_EAST", 17),
    ("HOOK_SOUTH", 18),
    ("CAN_NOT_DECAY", 19),
    ("ALLOW_DISTANCE_READ", 20),
    ("UNUSED", 21),
    ("CLIENT_CHARGES", 22),
    ("IGNORE_LOOK", 23),
    ("IS_ANIMATION", 24),
    ("FULL_GROUND", 25),
    ("FORCE_USE", 26),
)

# Bit -> ServerItem property (setFlags in ServerItem.as)
OTB_FLAG_MODEL = {
    "UNPASSABLE": "unpassable",
    "BLOCK_MISSILES": "blockMissiles",
    "BLOCK_PATHFINDER": "blockPathfinder",
    "HAS_ELEVATION": "hasElevation",
    "MULTI_USE": "multiUse",
    "PICKUPABLE": "pickupable",
    "MOVABLE": "movable",
    "STACKABLE": "stackable",
    "STACK_ORDER": "hasStackOrder",
    "READABLE": "readable",
    "ROTATABLE": "rotatable",
    "HANGABLE": "hangable",
    "HOOK_EAST": "hookEast",
    "HOOK_SOUTH": "hookSouth",
    "ALLOW_DISTANCE_READ": "allowDistanceRead",
    "CLIENT_CHARGES": "hasCharges",
    "IGNORE_LOOK": "ignoreLook",
    "IS_ANIMATION": "isAnimation",
    "FULL_GROUND": "fullGround",
    "FORCE_USE": "forceUse",
}

OTB_ATTR_NAMES = {
    0x10: "SERVER_ID",
    0x11: "CLIENT_ID",
    0x12: "NAME",
    0x13: "DESCRIPTION",
    0x14: "GROUND_SPEED",
    0x15: "SLOT",
    0x16: "MAX_ITEMS",
    0x17: "WEIGHT",
    0x18: "WEAPON",
    0x19: "AMMUNITION",
    0x1A: "ARMOR",
    0x1B: "MAGIC_LEVEL",
    0x1C: "MAGIC_FIELD_TYPE",
    0x1D: "LEVEL",
    0x1E: "SHOOT_TYPE",
    0x1F: "EFFECT",
    0x20: "SPRITE_HASH",
    0x21: "MINIMAP_COLOR",
    0x22: "MAX_READ_WRITE_CHARS",
    0x23: "MAX_READ_CHARS",
    0x2A: "LIGHT",
    0x2B: "STACK_ORDER",
    0x2D: "TRADE_AS",
}


class ServerItem:
    __slots__ = (
        "group_raw",
        "flags_raw",
        "model_flags",
        "other_bits",
        "attributes",
        "raw_attributes",
    )

    def __init__(self):
        self.group_raw = 0
        self.flags_raw = 0
        self.model_flags = {}
        self.other_bits = []
        self.attributes = {}
        self.raw_attributes = []

    @property
    def server_id(self):
        return self.attributes.get("serverId")

    @property
    def client_id(self):
        return self.attributes.get("clientId")

    def to_json(self):
        return {
            "serverId": self.server_id,
            "clientId": self.client_id,
            "group": OTB_GROUPS.get(self.group_raw, "unknown"),
            "groupRaw": self.group_raw,
            "flags": dict(self.model_flags),
            "flagsRaw": self.flags_raw,
            "flagsRawHex": "0x%08X" % self.flags_raw,
            "unmodeledFlagBits": list(self.other_bits),
            "attributes": dict(self.attributes),
            "rawAttributes": list(self.raw_attributes),
        }


class OtbFile:
    """items.otb parser (nodes + escaped byte strings)."""

    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        if len(self.data) < 8:
            raise ParseError("%s is too small to be an OTB file" % self.path)
        self.header = int.from_bytes(self.data[0:4], "little")
        self.root_attr_flags = 0
        self.version_major = 0
        self.version_minor = 0
        self.version_build = 0
        self.csd_version = ""
        self.items = []
        self._current = 0

    # -- node machinery (ported from OtbReader.as) -------------------------

    def _advance(self):
        data = self.data
        size = len(data)
        if self._current == 0:
            seek = 4
            if seek >= size or data[seek] != OTB_NODE_START:
                return False
            self._current = seek
            return True
        pos = self._current
        if pos >= size or data[pos] != OTB_NODE_START:
            return False
        pos += 2  # node start + node type
        while pos < size:
            value = data[pos]
            pos += 1
            if value == OTB_NODE_END:
                return False
            if value == OTB_NODE_START:
                self._current = pos - 1
                return True
            if value == OTB_ESCAPE:
                pos += 1
        return False

    def _node_data(self):
        data = self.data
        size = len(data)
        pos = self._current
        if pos >= size or data[pos] != OTB_NODE_START:
            return None
        pos += 1
        out = bytearray()
        while pos < size:
            value = data[pos]
            pos += 1
            if value == OTB_NODE_END or value == OTB_NODE_START:
                break
            if value == OTB_ESCAPE:
                if pos < size:
                    value = data[pos]
                    pos += 1
            out.append(value)
        return bytes(out)

    def _get_root_node(self):
        if not self._advance():
            return None
        return self._node_data()

    def _get_child_node(self):
        if not self._advance():
            return None
        return self._node_data()

    def _get_next_node(self):
        data = self.data
        size = len(data)
        pos = self._current
        if pos >= size or data[pos] != OTB_NODE_START:
            return None
        pos += 2  # node start + node type
        level = 1
        while pos < size:
            value = data[pos]
            pos += 1
            if value == OTB_NODE_END:
                level -= 1
                if level == 0:
                    if pos >= size:
                        return None
                    value = data[pos]
                    if value == OTB_NODE_END:
                        return None
                    if value != OTB_NODE_START:
                        return None
                    self._current = pos
                    return self._node_data()
            elif value == OTB_NODE_START:
                level += 1
            elif value == OTB_ESCAPE:
                pos += 1
        return None

    # -- high level --------------------------------------------------------

    def parse(self):
        root = self._get_root_node()
        if root is None:
            raise ParseError(
                "no OTB root node found at offset 4 (bad or compressed OTB file)"
            )
        self._parse_root(root)

        node = self._get_child_node()
        guard = 0
        while node is not None:
            guard += 1
            if guard > 1_000_000:
                raise ParseError("OTB node loop guard tripped")
            self.items.append(self._parse_item_node(node))
            node = self._get_next_node()
        return self.items

    def _parse_root(self, root):
        if len(root) < 5:
            raise ParseError("OTB root node too small (%d bytes)" % len(root))
        pos = 1  # byte 0 is the node type (0)
        self.root_attr_flags = int.from_bytes(root[pos : pos + 4], "little")
        pos += 4
        while pos < len(root):
            attr = root[pos]
            pos += 1
            if pos + 2 > len(root):
                raise ParseError("OTB root attribute %d truncated" % attr)
            length = root[pos] | (root[pos + 1] << 8)
            pos += 2
            payload = root[pos : pos + length]
            if len(payload) != length:
                raise ParseError("OTB root attribute %d truncated" % attr)
            pos += length
            if attr == 0x01 and length >= 12:
                self.version_major = int.from_bytes(payload[0:4], "little")
                self.version_minor = int.from_bytes(payload[4:8], "little")
                self.version_build = int.from_bytes(payload[8:12], "little")
                csd = payload[12:140]
                self.csd_version = csd.split(b"\x00", 1)[0].decode("latin-1", "replace")

    def _parse_item_node(self, node):
        if len(node) < 5:
            raise ParseError("OTB item node too small (%d bytes)" % len(node))
        item = ServerItem()
        item.group_raw = node[0]
        item.flags_raw = int.from_bytes(node[1:5], "little")
        item.model_flags = _decode_otb_flags(item.flags_raw, item.other_bits)

        pos = 5
        size = len(node)
        while pos < size:
            attr = node[pos]
            pos += 1
            if pos + 2 > size:
                raise ParseError("OTB attribute 0x%02X truncated" % attr)
            length = node[pos] | (node[pos + 1] << 8)
            pos += 2
            payload = node[pos : pos + length]
            if len(payload) != length:
                raise ParseError("OTB attribute 0x%02X truncated" % attr)
            pos += length
            name = OTB_ATTR_NAMES.get(attr, "UNKNOWN_0x%02X" % attr)
            item.raw_attributes.append(
                {
                    "code": "0x%02X" % attr,
                    "name": name,
                    "length": length,
                    "data": payload.hex(),
                }
            )
            _decode_otb_attribute(item, attr, payload)

        if item.server_id is None:
            raise ParseError("OTB item without SERVER_ID attribute")
        return item


def _decode_otb_flags(flags, other_bits):
    model = {}
    for name, bit in OTB_FLAG_BITS:
        value = bool(flags & (1 << bit))
        prop = OTB_FLAG_MODEL.get(name)
        if prop:
            model[prop] = value
        elif value:
            other_bits.append(name)
    return model


def _decode_otb_attribute(item, attr, payload):
    attrs = item.attributes
    if attr == 0x10 and len(payload) >= 2:
        attrs["serverId"] = int.from_bytes(payload[0:2], "little")
    elif attr == 0x11 and len(payload) >= 2:
        attrs["clientId"] = int.from_bytes(payload[0:2], "little")
    elif attr == 0x12:
        attrs["name"] = _decode_text(payload)
    elif attr == 0x13:
        attrs["description"] = _decode_text(payload)
    elif attr == 0x14 and len(payload) >= 2:
        attrs["groundSpeed"] = int.from_bytes(payload[0:2], "little")
    elif attr == 0x20:
        attrs["spriteHash"] = payload.hex()
    elif attr == 0x21 and len(payload) >= 2:
        attrs["minimapColor"] = int.from_bytes(payload[0:2], "little")
    elif attr == 0x22 and len(payload) >= 2:
        attrs["maxReadWriteChars"] = int.from_bytes(payload[0:2], "little")
    elif attr == 0x23 and len(payload) >= 2:
        attrs["maxReadChars"] = int.from_bytes(payload[0:2], "little")
    elif attr == 0x2A and len(payload) >= 4:
        attrs["lightLevel"] = int.from_bytes(payload[0:2], "little")
        attrs["lightColor"] = int.from_bytes(payload[2:4], "little")
    elif attr == 0x2B and len(payload) >= 1:
        attrs["stackOrder"] = payload[0]
    elif attr == 0x2D and len(payload) >= 2:
        attrs["tradeAs"] = int.from_bytes(payload[0:2], "little")


def _decode_text(payload):
    if not payload:
        return ""
    try:
        return payload.decode("utf-8")
    except UnicodeDecodeError:
        return payload.decode("latin-1")


# --------------------------------------------------------------------------
# Application context + commands
# --------------------------------------------------------------------------


class Context:
    def __init__(self, args):
        self.args = args
        self.files = {"dat": args.dat, "spr": args.spr, "otb": args.otb}
        self.warnings = []
        self.dat = None
        self.spr = None
        self.otb = None
        self.version_row = None
        self.version_matches = []
        self.dat_features = None
        self.dat_detection = None
        self.dat_header = None
        self.dat_collected = {}
        self.dat_max_sprite_id = None
        self.spr_count = None
        self.transparency = None
        self.transparency_detail = None
        self._load_version()

    # -- version -----------------------------------------------------------

    def _load_version(self):
        dat_sig = spr_sig = None
        if self.args.dat:
            data = Path(self.args.dat).read_bytes()
            if len(data) < 4:
                raise ParseError("DAT file %s is too small" % self.args.dat)
            dat_sig = int.from_bytes(data[0:4], "little")
        if self.args.spr:
            handle = open(self.args.spr, "rb")
            try:
                sig = handle.read(4)
            finally:
                handle.close()
            if len(sig) < 4:
                raise ParseError("SPR file %s is too small" % self.args.spr)
            spr_sig = int.from_bytes(sig, "little")

        if self.args.force_version:
            row = find_version_by_label(self.args.force_version)
            if row is None:
                raise ParseError(
                    "unknown --force-version %r" % self.args.force_version
                )
            self.version_row = row
            self.version_matches = [row]
            self.warnings.append(
                "version forced to %s (dat=%s spr=%s)"
                % (
                    row[1],
                    format_sig(dat_sig) if dat_sig is not None else "-",
                    format_sig(spr_sig) if spr_sig is not None else "-",
                )
            )
            return

        if dat_sig is None and spr_sig is None:
            return

        matches = find_versions(dat_sig, spr_sig)
        if not matches and dat_sig is not None and spr_sig is not None:
            dat_only = find_versions(dat_sig, None)
            spr_only = find_versions(None, spr_sig)
            if dat_only:
                matches = dat_only
                self.warnings.append(
                    "DAT/SPR signature pair is not a known combination; matched on DAT"
                )
            elif spr_only:
                matches = spr_only
                self.warnings.append("matched on SPR signature only")
        if not matches:
            parts = []
            if dat_sig is not None:
                parts.append("dat=%s" % format_sig(dat_sig))
            if spr_sig is not None:
                parts.append("spr=%s" % format_sig(spr_sig))
            raise ParseError(
                "unknown client version (%s); use --force-version to override"
                % ", ".join(parts)
            )
        self.version_matches = matches
        self.version_row = matches[0]
        if len(matches) > 1:
            self.warnings.append(
                "signature matches multiple releases: %s"
                % ", ".join(row[1] for row in matches)
            )

    @property
    def version_value(self):
        return self.version_row[0] if self.version_row else 1098

    @property
    def version_label(self):
        return self.version_row[1] if self.version_row else "unknown"

    def version_json(self):
        row = self.version_row
        return {
            "label": self.version_label,
            "value": self.version_value,
            "datSignature": format_sig(row[2]) if row else None,
            "sprSignature": format_sig(row[3]) if row else None,
            "otbVersionHint": row[4] if row else None,
            "matches": [r[1] for r in self.version_matches],
        }

    # -- lazy loaders ------------------------------------------------------

    def load_dat(self):
        if self.dat is None:
            if not self.args.dat:
                raise ParseError("this command requires --dat")
            self.dat = DatFile(self.args.dat)
        return self.dat

    def load_spr(self):
        if self.spr is None:
            if not self.args.spr:
                raise ParseError("this command requires --spr")
            self.spr = SprFile(self.args.spr)
        return self.spr

    def ensure_spr_count(self):
        if self.spr_count is not None:
            return self.spr_count
        if not self.args.spr:
            return None
        spr = self.load_spr()
        if self.args.extended is not None:
            extended = self.args.extended
        elif self.dat_features is not None:
            extended = self.dat_features["extended"]
        else:
            extended = spr.detect_extended(self.version_value)
        self.spr_count = spr.init(extended)
        return self.spr_count

    def detect_dat_features(self, collect=None):
        """Detect (and validate) the DAT feature set; caches the result."""
        if self.dat_features is None:
            if not self.args.dat:
                return None
            dat = self.load_dat()
            overrides = {
                "extended": self.args.extended,
                "improvedAnimations": self.args.improved_animations,
                "frameGroups": self.args.frame_groups,
            }
            spr_count = None
            if self.args.spr:
                spr_count = self.ensure_spr_count()
            detection = dat.detect(
                self.version_value, overrides, spr_count=spr_count, collect=collect
            )
            if detection["validated"]:
                self.dat_features = detection["features"]
                self.dat_collected.update(detection["collected"] or {})
                self.dat_max_sprite_id = detection["maxSpriteId"]
            else:
                heuristic = detection["heuristic"]
                self.warnings.append(
                    "no DAT feature combination parsed cleanly; using version "
                    "heuristics (extended=%(extended)s improvedAnimations=%(improvedAnimations)s "
                    "frameGroups=%(frameGroups)s)" % heuristic
                )
                self.dat_features = {
                    "extended": heuristic["extended"],
                    "improvedAnimations": heuristic["improvedAnimations"],
                    "frameGroups": heuristic["frameGroups"],
                }
            self.dat_detection = detection
            self.dat_header = dat.header()
        elif collect:
            missing = [key for key in collect if key not in self.dat_collected]
            if missing:
                self._collect(missing)
        return self.dat_features

    def _collect(self, keys):
        collect = {key: None for key in keys}
        _, collected, max_sprite_id = self.dat.parse(
            self.version_value,
            self.dat_features,
            collect=collect,
            spr_count=self.spr_count,
        )
        self.dat_collected.update(collected)
        if max_sprite_id is not None:
            self.dat_max_sprite_id = max_sprite_id

    def collect_dat_things(self, keys):
        """Return {key: Thing} for (category, id) keys."""
        self.detect_dat_features(collect={key: None for key in keys})
        missing = [key for key in keys if key not in self.dat_collected]
        if missing:
            self._collect(missing)
        return {key: self.dat_collected[key] for key in keys}

    def resolve_transparency(self):
        """Resolve the transparency flag (CLI override > SPR probe > heuristic)."""
        if self.transparency is not None:
            return self.transparency
        if self.args.transparency is not None:
            self.transparency = self.args.transparency
            self.transparency_detail = {"mode": "rgba" if self.transparency else "rgb",
                                        "confidence": "cli override"}
            return self.transparency
        spr = self.load_spr()
        detected, detail = spr.detect_transparency()
        if detected is not None:
            detail["source"] = "spr validation"
            self.transparency = detected
            self.transparency_detail = detail
            return detected
        fallback = self.version_value >= 970
        detail["source"] = "version heuristic"
        detail["heuristicValue"] = fallback
        self.warnings.append(
            "transparency could not be validated from the SPR file; using "
            "version heuristic (transparency=%s)" % fallback
        )
        self.transparency = fallback
        self.transparency_detail = detail
        return fallback

    # -- json helpers ------------------------------------------------------

    def features_json(self):
        detection = self.dat_detection or {}
        features = self.dat_features or {}
        extended = (
            self.args.extended
            if self.args.extended is not None
            else features.get("extended")
        )
        if extended is None and self.spr is not None:
            extended = self.spr.extended
        return {
            "extended": extended,
            "transparency": self.transparency,
            "improvedAnimations": (
                self.args.improved_animations
                if self.args.improved_animations is not None
                else features.get("improvedAnimations")
            ),
            "frameGroups": (
                self.args.frame_groups
                if self.args.frame_groups is not None
                else features.get("frameGroups")
            ),
            "detection": {
                "datValidated": bool(detection.get("validated")),
                "reader": detection.get("reader", "MetadataReader%d" % pick_reader_kind(self.version_value)),
                "attempts": detection.get("attempts", []),
                "transparency": self.transparency_detail,
            },
        }

    def counts_json(self):
        counts = {}
        if self.dat_header:
            counts.update(
                {
                    "minItemId": MIN_ITEM_ID,
                    "maxItemId": self.dat_header["items"],
                    "items": self.dat_header["items"] - MIN_ITEM_ID + 1,
                    "outfits": self.dat_header["outfits"],
                    "effects": self.dat_header["effects"],
                    "missiles": self.dat_header["missiles"],
                }
            )
        if self.spr_count is not None:
            counts["sprites"] = self.spr_count
        if self.otb is not None:
            counts["otbItems"] = len(self.otb.items)
        return counts


def load_otb(ctx):
    if ctx.otb is None:
        if not ctx.args.otb:
            raise ParseError("this command requires --otb")
        otb = OtbFile(ctx.args.otb)
        otb.parse()
        ctx.otb = otb
    return ctx.otb


def export_sprites(ctx, sprite_ids, out_dir, base_name, sheet=False):
    spr = ctx.load_spr()
    ctx.ensure_spr_count()
    transparent = ctx.resolve_transparency()
    out_path = Path(out_dir)
    out_path.mkdir(parents=True, exist_ok=True)

    files = []
    pixels_list = []
    for sprite_id in sprite_ids:
        if sprite_id == 0:
            continue
        if sprite_id > spr.sprite_count:
            ctx.warnings.append(
                "sprite id %d is out of range (max %d); skipped"
                % (sprite_id, spr.sprite_count)
            )
            continue
        compressed = spr.read_compressed(sprite_id)
        file_path = out_path / ("sprite_%d.png" % sprite_id)
        if compressed is None:
            pixels = bytes(DEFAULT_SPRITE_SIZE * DEFAULT_SPRITE_SIZE * 4)
            write_sprite_png(file_path, pixels)
            files.append(
                {"spriteId": sprite_id, "file": str(file_path), "empty": True,
                 "compressedLength": 0}
            )
        else:
            data, length, address = compressed
            pixels, visible, total = spr.decode(data, transparent)
            write_sprite_png(file_path, pixels)
            files.append(
                {
                    "spriteId": sprite_id,
                    "file": str(file_path),
                    "empty": length == 0,
                    "compressedLength": length,
                    "address": address,
                    "visiblePixels": visible,
                    "compressedSha1": hashlib.sha1(data).hexdigest(),
                }
            )
        pixels_list.append(pixels)

    sheet_file = None
    if sheet and pixels_list:
        width, height, canvas = compose_sheet(pixels_list)
        sheet_file = out_path / ("%s_sheet.png" % base_name)
        Path(sheet_file).write_bytes(encode_png(width, height, canvas))

    return {
        "outDir": str(out_path),
        "transparentClient": transparent,
        "sprites": files,
        "sheet": str(sheet_file) if sheet_file else None,
        "sheetOrder": [f["spriteId"] for f in files],
    }


# -- commands --------------------------------------------------------------


def cmd_info(ctx):
    result = {}
    if ctx.args.dat:
        ctx.load_dat()
        ctx.detect_dat_features()
    if ctx.args.spr:
        ctx.ensure_spr_count()
        ctx.resolve_transparency()
    if ctx.args.otb:
        otb = load_otb(ctx)
        result["otb"] = {
            "headerVersion": otb.header,
            "version": "%d.%d.%d"
            % (otb.version_major, otb.version_minor, otb.version_build),
            "csdVersion": otb.csd_version,
            "itemCount": len(otb.items),
        }
    return result


def _thing_out_of_range(ctx, category, thing_id):
    first_id, last_id = dat_thing_range(ctx.dat_header)[category]
    return ParseError(
        "%s id %d is out of range %d..%d" % (category, thing_id, first_id, last_id)
    )


def cmd_client(ctx):
    if not ctx.args.dat:
        raise ParseError("the 'client' command requires --dat")
    category = ctx.args.category
    thing_id = ctx.args.id
    things = ctx.collect_dat_things({(category, thing_id): None})
    thing = things.get((category, thing_id))
    if thing is None:
        raise _thing_out_of_range(ctx, category, thing_id)

    result = {"clientThing": thing.to_json()}

    if category == "item" and ctx.args.otb:
        otb = load_otb(ctx)
        matches = [item for item in otb.items if item.client_id == thing_id]
        result["otbMapping"] = {
            "serverIds": [item.server_id for item in matches],
            "serverItems": [item.to_json() for item in matches],
        }

    if ctx.args.dump_sprites:
        result["spriteExport"] = export_sprites(
            ctx,
            thing_sprite_ids(thing),
            ctx.args.dump_sprites,
            "%s%d" % (category, thing_id),
            sheet=ctx.args.sheet,
        )
    return result


def cmd_server(ctx):
    otb = load_otb(ctx)
    matches = [item for item in otb.items if item.server_id == ctx.args.id]
    if not matches:
        raise ParseError(
            "server item id %d not found in %s (%d items parsed)"
            % (ctx.args.id, ctx.args.otb, len(otb.items))
        )
    if len(matches) > 1:
        ctx.warnings.append(
            "server item id %d appears %d times; using the first"
            % (ctx.args.id, len(matches))
        )
    item = matches[0]
    result = {
        "serverItem": item.to_json(),
        "additionalMatches": [m.to_json() for m in matches[1:]],
    }

    thing = None
    client_id = item.client_id
    if ctx.args.dat and client_id:
        thing = ctx.collect_dat_things({("item", client_id): None}).get(("item", client_id))
        if thing is not None:
            result["clientThing"] = thing.to_json()
        else:
            ctx.warnings.append(
                "client id %d (from OTB) is outside the DAT item range" % client_id
            )
    elif ctx.args.dat and not client_id:
        ctx.warnings.append("OTB item has no CLIENT_ID attribute")

    if ctx.args.dump_sprites:
        if thing is None:
            raise ParseError(
                "cannot export sprites: no DAT thing resolved (provide --dat and "
                "an OTB item with CLIENT_ID)"
            )
        result["spriteExport"] = export_sprites(
            ctx,
            thing_sprite_ids(thing),
            ctx.args.dump_sprites,
            "server%d" % ctx.args.id,
            sheet=ctx.args.sheet,
        )
    return result


def cmd_sprite(ctx):
    spr = ctx.load_spr()
    ctx.ensure_spr_count()
    sprite_id = ctx.args.id
    compressed = spr.read_compressed(sprite_id)
    transparent = ctx.resolve_transparency()
    if compressed is None:
        sprite = {
            "id": sprite_id,
            "empty": True,
            "address": 0,
            "compressedLength": 0,
        }
        pixels = bytes(DEFAULT_SPRITE_SIZE * DEFAULT_SPRITE_SIZE * 4)
    else:
        data, length, address = compressed
        pixels, visible, total = spr.decode(data, transparent)
        sprite = {
            "id": sprite_id,
            "empty": length == 0,
            "address": address,
            "compressedLength": length,
            "visiblePixels": visible,
            "compressedSha1": hashlib.sha1(data).hexdigest(),
            "transparentClient": transparent,
        }
    result = {
        "sprite": sprite,
        "spriteCount": spr.sprite_count,
    }
    if ctx.args.out:
        out_path = Path(ctx.args.out)
        out_path.mkdir(parents=True, exist_ok=True)
        file_path = out_path / ("sprite_%d.png" % sprite_id)
        write_sprite_png(file_path, pixels)
        result["pngFile"] = str(file_path)
    return result


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------


def build_parser():
    parser = argparse.ArgumentParser(
        prog=TOOL_NAME,
        description="Inspect Tibia DAT/SPR/OTB files and print JSON.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  %(prog)s --dat Tibia.dat --spr Tibia.spr --otb items.otb info\n"
            "  %(prog)s --dat Tibia.dat --spr Tibia.spr --otb items.otb client 930\n"
            "  %(prog)s --dat Tibia.dat --spr Tibia.spr --otb items.otb server 930\n"
            "  %(prog)s --dat Tibia.dat --spr Tibia.spr client 930 --dump-sprites out/\n"
            "  %(prog)s --spr Tibia.spr sprite 1234 --out out/\n"
        ),
    )
    parser.add_argument("--dat", metavar="FILE", help="path to Tibia.dat")
    parser.add_argument("--spr", metavar="FILE", help="path to Tibia.spr")
    parser.add_argument("--otb", metavar="FILE", help="path to items.otb")
    parser.add_argument("--compact", action="store_true", help="compact JSON output")
    parser.add_argument(
        "--output", metavar="FILE",
        help="write the JSON document to FILE instead of stdout",
    )
    parser.add_argument(
        "--force-version", metavar="LABEL",
        help="override version detection (e.g. '10.98')",
    )
    parser.add_argument(
        "--extended", action=argparse.BooleanOptionalAction, default=None,
        help="override the extended flag (uint32 sprite ids)",
    )
    parser.add_argument(
        "--transparency", action=argparse.BooleanOptionalAction, default=None,
        help="override the transparent sprite format (RGBA pixels)",
    )
    parser.add_argument(
        "--improved-animations", action=argparse.BooleanOptionalAction, default=None,
        help="override the improved animations flag",
    )
    parser.add_argument(
        "--frame-groups", action=argparse.BooleanOptionalAction, default=None,
        help="override the frame groups flag",
    )
    parser.add_argument("--debug", action="store_true", help="print tracebacks")

    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("info", help="detected version, feature flags and counts")

    client = sub.add_parser("client", help="query a DAT thing by id (client id)")
    client.add_argument("id", type=int)
    client.add_argument("--category", choices=CATEGORIES, default="item")
    client.add_argument(
        "--dump-sprites", metavar="DIR",
        help="export the thing's sprites as PNG files",
    )
    client.add_argument(
        "--sheet", action="store_true",
        help="also write a sprite sheet PNG (requires --dump-sprites)",
    )

    server = sub.add_parser("server", help="query an OTB item by server id")
    server.add_argument("id", type=int)
    server.add_argument(
        "--dump-sprites", metavar="DIR",
        help="export the mapped DAT thing's sprites as PNG files",
    )
    server.add_argument(
        "--sheet", action="store_true",
        help="also write a sprite sheet PNG (requires --dump-sprites)",
    )

    sprite = sub.add_parser("sprite", help="query a SPR sprite by id")
    sprite.add_argument("id", type=int)
    sprite.add_argument("--out", metavar="DIR", help="export the sprite as PNG")

    return parser


def _emit(document, compact, output=None):
    if compact:
        text = json.dumps(document, separators=(",", ":"))
    else:
        text = json.dumps(document, indent=2)
    if output:
        Path(output).parent.mkdir(parents=True, exist_ok=True)
        Path(output).write_text(text + "\n", encoding="utf-8")
    else:
        sys.stdout.write(text + "\n")


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)

    ctx = None
    try:
        if getattr(args, "sheet", False) and not getattr(args, "dump_sprites", None):
            raise ParseError("--sheet requires --dump-sprites")
        ctx = Context(args)
        if args.command == "info":
            result = cmd_info(ctx)
        elif args.command == "client":
            result = cmd_client(ctx)
        elif args.command == "server":
            result = cmd_server(ctx)
        elif args.command == "sprite":
            result = cmd_sprite(ctx)
        else:  # pragma: no cover
            raise ParseError("unknown command %r" % args.command)

        # Report the transparency format whenever an SPR file is available.
        if args.spr and ctx.transparency is None:
            ctx.resolve_transparency()

        document = {
            "status": "ok",
            "tool": TOOL_NAME,
            "files": {key: (str(value) if value else None) for key, value in ctx.files.items()},
            "gameVersion": ctx.version_json(),
            "features": ctx.features_json(),
            "counts": ctx.counts_json(),
            "result": result,
            "warnings": ctx.warnings,
        }
        _emit(document, args.compact, args.output)
        return 0
    except ParseError as error:
        _emit_error(str(error), args, "ParseError")
        return 1
    except FileNotFoundError as error:
        _emit_error("file not found: %s" % error, args, "FileNotFoundError")
        return 1
    except Exception as error:  # pragma: no cover
        if args.debug:
            raise
        _emit_error("%s: %s" % (type(error).__name__, error), args, type(error).__name__)
        return 1
    finally:
        if ctx is not None and ctx.spr is not None:
            ctx.spr.close()


def _emit_error(message, args, error_type="Error"):
    document = {
        "status": "error",
        "tool": TOOL_NAME,
        "errorType": error_type,
        "error": message,
    }
    _emit(document, args.compact, args.output)


if __name__ == "__main__":
    sys.exit(main())
