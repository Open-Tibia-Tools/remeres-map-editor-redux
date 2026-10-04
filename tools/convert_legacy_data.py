#!/usr/bin/env python3
from __future__ import annotations

import argparse
import copy
import io
import json
import re
import shutil
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterable
import xml.etree.ElementTree as ET


PALETTE_ORDER = ["Terrain", "Doodad", "Collection", "Item", "Creature", "Raw"]
PALETTE_TOKEN_MAP = {
    "terrain": "Terrain",
    "collections": "Collection",
    "doodad": "Doodad",
    "items": "Item",
    "creatures": "Creature",
    "raw": "Raw",
}
MODULE_SECTIONS = (
    ("borders", "borders/", False),
    ("brushes", "brushes/", False),
    ("creatures", "creatures/", False),
    ("items", "items/", False),
    ("tilesets", "tilesets/", True),
)
ITEM_REGISTRY_FILES = ("items.xml", "items2.xml")
XML_DECLARATION = '<?xml version="1.0" encoding="utf-8"?>\n'
BARE_AMPERSAND_RE = re.compile(r"&(?!#\d+;|#x[0-9A-Fa-f]+;|[A-Za-z_][A-Za-z0-9._-]*;)")
WINDOWS_RESERVED_NAMES = {
    "CON",
    "PRN",
    "AUX",
    "NUL",
    "COM1",
    "COM2",
    "COM3",
    "COM4",
    "COM5",
    "COM6",
    "COM7",
    "COM8",
    "COM9",
    "LPT1",
    "LPT2",
    "LPT3",
    "LPT4",
    "LPT5",
    "LPT6",
    "LPT7",
    "LPT8",
    "LPT9",
}


@dataclass(slots=True)
class FileIssue:
    path: str
    messages: list[str] = field(default_factory=list)


@dataclass(slots=True)
class TopLevelNode:
    source_file: str
    root_tag: str
    tag: str
    sequence: int
    element: ET.Element


@dataclass(slots=True)
class LegacyVersionData:
    version: str
    version_dir: Path
    include_files: list[str]
    metaitems: list[int]
    borders: list[TopLevelNode]
    brushes: list[TopLevelNode]
    tilesets: list[TopLevelNode]
    items: list[TopLevelNode]
    creatures: list[TopLevelNode]
    items_otb: Path | None
    issues: list[FileIssue] = field(default_factory=list)


@dataclass(slots=True)
class ModularTileset:
    name: str
    wrapper: str
    file_path: str
    source_file: str
    sequence: int
    children: list[ET.Element]

    @property
    def palettes(self) -> list[str]:
        return wrapper_to_palettes(self.wrapper)


@dataclass(slots=True)
class NormalizedVersionData:
    version: str
    metaitems: list[int]
    borders: list[ET.Element]
    brushes: list[ET.Element]
    items: list[ET.Element]
    creatures: list[ET.Element]
    tilesets: list[ModularTileset]
    palette_includes: dict[str, list[str]]
    issues: list[FileIssue]
    validation: dict[str, object]
    items_otb: Path | None


def clone_element(element: ET.Element) -> ET.Element:
    return copy.deepcopy(element)


def strip_invalid_xml_chars(text: str) -> str:
    cleaned: list[str] = []
    for char in text:
        code = ord(char)
        if code in (0x9, 0xA, 0xD) or 0x20 <= code <= 0xD7FF or 0xE000 <= code <= 0xFFFD or 0x10000 <= code <= 0x10FFFF:
            cleaned.append(char)
    return "".join(cleaned)


def sanitize_xml_text(path: Path) -> tuple[str, list[str]]:
    raw = path.read_bytes()
    messages: list[str] = []

    if b"\x00" in raw:
        raw = raw.replace(b"\x00", b"")
        messages.append("removed NUL bytes")

    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        text = raw.decode("cp1252", errors="replace")
        messages.append("decoded with cp1252 fallback")

    stripped = strip_invalid_xml_chars(text)
    if stripped != text:
        text = stripped
        messages.append("removed invalid XML control characters")

    escaped_ampersands = BARE_AMPERSAND_RE.sub("&amp;", text)
    if escaped_ampersands != text:
        text = escaped_ampersands
        messages.append("escaped malformed ampersands")

    if text.startswith("\ufeff"):
        text = text.lstrip("\ufeff")
        messages.append("removed UTF-8 BOM")

    return text, messages


def parse_top_level_elements(path: Path) -> tuple[str, list[ET.Element], list[str]]:
    text, messages = sanitize_xml_text(path)
    root_tag: str | None = None
    root_element: ET.Element | None = None
    top_level: list[ET.Element] = []
    stack: list[ET.Element] = []

    try:
        for event, elem in ET.iterparse(io.StringIO(text), events=("start", "end")):
            if event == "start":
                stack.append(elem)
                if root_tag is None:
                    root_tag = elem.tag
                    root_element = elem
                continue

            if len(stack) == 2:
                top_level.append(clone_element(elem))
                if root_element is not None:
                    try:
                        root_element.remove(elem)
                    except ValueError:
                        pass
                elem.clear()

            stack.pop()
    except ET.ParseError as exc:
        raise RuntimeError(f"{path}: parse failed after sanitization: {exc}") from exc

    if root_tag is None:
        raise RuntimeError(f"{path}: XML document is empty")

    return root_tag, top_level, messages


def parse_root(path: Path) -> tuple[ET.Element, list[str]]:
    text, messages = sanitize_xml_text(path)
    try:
        root = ET.fromstring(text)
    except ET.ParseError as exc:
        raise RuntimeError(f"{path}: parse failed after sanitization: {exc}") from exc
    return root, messages


def expand_item_ids(element: ET.Element) -> list[int]:
    if "id" in element.attrib:
        return [int(element.attrib["id"])]
    if "fromid" not in element.attrib:
        return []
    start = int(element.attrib["fromid"])
    end = int(element.attrib.get("toid", element.attrib["fromid"]))
    if end < start:
        start, end = end, start
    return list(range(start, end + 1))


def item_coverage(elements: Iterable[ET.Element]) -> set[int]:
    covered: set[int] = set()
    for element in elements:
        covered.update(expand_item_ids(element))
    return covered


def wrapper_to_palettes(wrapper: str) -> list[str]:
    if wrapper in PALETTE_TOKEN_MAP:
        return [PALETTE_TOKEN_MAP[wrapper]]

    palettes: list[str] = []
    for token in wrapper.split("_and_"):
        palette = PALETTE_TOKEN_MAP.get(token)
        if palette and palette not in palettes:
            palettes.append(palette)
    return palettes


def encode_filename_component(text: str) -> str:
    text = re.sub(r"\s+", " ", text.replace("/", " ").replace("\\", " ")).strip()
    encoded: list[str] = []
    for index, char in enumerate(text):
        code = ord(char)
        safe = char.isalnum() or char in (" ", "-", "_", "(", ")", ".", ",", "&", "'")
        if safe and char not in '<>:"/\\|?*':
            if index == 0 and char == " ":
                encoded.append(f"~{code:04x}")
            elif index == len(text) - 1 and char in (" ", "."):
                encoded.append(f"~{code:04x}")
            else:
                encoded.append(char)
        else:
            encoded.append(f"~{code:04x}")

    result = "".join(encoded).strip()
    if not result:
        result = "unnamed"
    if result.upper() in WINDOWS_RESERVED_NAMES:
        result = f"{result}~"
    return result


def compact_item_elements(item_ids: Iterable[int]) -> list[ET.Element]:
    sorted_ids = sorted(set(item_ids))
    if not sorted_ids:
        return []

    result: list[ET.Element] = []
    start = previous = sorted_ids[0]
    for item_id in sorted_ids[1:]:
        if item_id == previous + 1:
            previous = item_id
            continue
        result.append(range_item_element(start, previous))
        start = previous = item_id
    result.append(range_item_element(start, previous))
    return result


def range_item_element(start: int, end: int) -> ET.Element:
    if start == end:
        return ET.Element("item", {"id": str(start)})
    return ET.Element("item", {"fromid": str(start), "toid": str(end)})


def indent_tree(element: ET.Element) -> None:
    ET.indent(element, space="    ")


def write_xml(path: Path, root: ET.Element) -> None:
    indent_tree(root)
    xml_text = ET.tostring(root, encoding="unicode")
    path.write_text(XML_DECLARATION + xml_text + "\n", encoding="utf-8")


def find_case_insensitive_file(directory: Path, filename: str) -> Path | None:
    exact = directory / filename
    if exact.exists():
        return exact
    lower = filename.lower()
    try:
        for child in directory.iterdir():
            if child.name.lower() == lower:
                return child
    except (OSError, PermissionError):
        pass
    return None


def is_valid_folder_name(name: str) -> bool:
    if not name or not name.strip():
        return False
    stripped = name.strip()
    if stripped.startswith("<") or stripped.endswith(">"):
        return False
    illegal = set('<>:"/\\|?*\0')
    if any(c in illegal for c in stripped):
        return False
    if stripped in (".", ".."):
        return False
    return True


def resolve_version_root(path: Path) -> Path:
    """If user pointed to a materials/ subdirectory, resolve to the true version root if items/ or creatures/ exist in parent."""
    if path.name.lower() == "materials" and path.parent.is_dir():
        if (path.parent / "items").is_dir() or (path.parent / "creatures").is_dir():
            return path.parent
    return path


def is_version_dir(path: Path) -> bool:
    if not path.is_dir():
        return False
    if find_case_insensitive_file(path, "materials.xml") is not None:
        return True
    materials_sub = path / "materials"
    if materials_sub.is_dir() and find_case_insensitive_file(materials_sub, "materials.xml") is not None:
        return True
    return False


def resolve_include_path(base_file: Path, inc_file: str, materials_dir: Path, version_dir: Path) -> Path | None:
    # 1. Try relative to the file containing the include
    p1 = find_case_insensitive_file(base_file.parent, inc_file)
    if p1 and p1.exists():
        return p1
    exact1 = (base_file.parent / inc_file).resolve()
    if exact1.exists():
        return exact1

    # 2. Try relative to materials_dir
    p2 = find_case_insensitive_file(materials_dir, inc_file)
    if p2 and p2.exists():
        return p2
    exact2 = (materials_dir / inc_file).resolve()
    if exact2.exists():
        return exact2

    # 3. Try relative to version_dir
    p3 = find_case_insensitive_file(version_dir, inc_file)
    if p3 and p3.exists():
        return p3
    exact3 = (version_dir / inc_file).resolve()
    if exact3.exists():
        return exact3

    return None


class LegacyVersionReader:
    def __init__(self, base_dir: Path) -> None:
        self.base_dir = resolve_version_root(base_dir)

    def discover_versions(self) -> list[Path]:
        resolved = resolve_version_root(self.base_dir)
        if is_version_dir(resolved):
            return [resolved]
        if not resolved.is_dir():
            return []
        versions: list[Path] = []
        for path in resolved.iterdir():
            if path.is_dir() and is_version_dir(path):
                versions.append(path)
        return sorted(
            versions,
            key=lambda path: (0 if path.name.isdigit() else 1, int(path.name) if path.name.isdigit() else path.name),
        )

    def read(
        self,
        version_dir: Path,
        log_cb: Callable[[str, str], None] | None = None,
    ) -> LegacyVersionData:
        version_dir = resolve_version_root(version_dir)
        issues: list[FileIssue] = []
        if log_cb:
            log_cb("INFO", f"[{version_dir.name}] Reading legacy data from {version_dir}")

        materials_path = find_case_insensitive_file(version_dir, "materials.xml")
        if not materials_path or not materials_path.exists():
            materials_sub = version_dir / "materials"
            if materials_sub.is_dir():
                materials_path = find_case_insensitive_file(materials_sub, "materials.xml")
        if not materials_path or not materials_path.exists():
            raise RuntimeError(f"{version_dir}: missing materials.xml")

        materials_dir = materials_path.parent

        include_files: list[str] = []
        metaitems: list[int] = []
        borders: list[TopLevelNode] = []
        brushes: list[TopLevelNode] = []
        tilesets: list[TopLevelNode] = []
        sequence = 0
        visited_includes: set[Path] = set()

        def process_include(file_path: Path) -> None:
            nonlocal sequence
            canon = file_path.resolve()
            if canon in visited_includes or not canon.exists():
                return
            visited_includes.add(canon)
            try:
                rel_path = str(file_path.relative_to(version_dir)).replace("\\", "/")
            except ValueError:
                rel_path = file_path.name
            include_files.append(rel_path)

            try:
                root_tag, elements, include_messages = parse_top_level_elements(file_path)
            except Exception as exc:
                issues.append(FileIssue(path=rel_path, messages=[str(exc)]))
                if log_cb:
                    log_cb("ERROR", f"[{version_dir.name}] {rel_path}: {exc}")
                return

            if include_messages:
                issues.append(FileIssue(path=rel_path, messages=include_messages))
                if log_cb:
                    for msg in include_messages:
                        log_cb("WARN", f"[{version_dir.name}] {rel_path}: {msg}")

            for element in elements:
                if element.tag == "border":
                    borders.append(
                        TopLevelNode(
                            source_file=rel_path,
                            root_tag=root_tag,
                            tag=element.tag,
                            sequence=sequence,
                            element=element,
                        )
                    )
                    sequence += 1
                elif element.tag == "brush":
                    brushes.append(
                        TopLevelNode(
                            source_file=rel_path,
                            root_tag=root_tag,
                            tag=element.tag,
                            sequence=sequence,
                            element=element,
                        )
                    )
                    sequence += 1
                elif element.tag == "tileset":
                    tilesets.append(
                        TopLevelNode(
                            source_file=rel_path,
                            root_tag=root_tag,
                            tag=element.tag,
                            sequence=sequence,
                            element=element,
                        )
                    )
                    sequence += 1
                elif element.tag == "metaitem" and "id" in element.attrib:
                    try:
                        metaitems.append(int(element.attrib["id"]))
                    except ValueError:
                        pass
                elif element.tag == "include" and "file" in element.attrib:
                    target = resolve_include_path(file_path, element.attrib["file"], materials_dir, version_dir)
                    if target and target.exists():
                        process_include(target)
                    else:
                        missing_str = element.attrib["file"]
                        issues.append(FileIssue(path=rel_path, messages=[f"Included file not found: {missing_str}"]))
                        if log_cb:
                            log_cb("WARN", f"[{version_dir.name}] {rel_path}: Included file not found: {missing_str}")

        process_include(materials_path)

        # Ingest Items
        items: list[TopLevelNode] = []
        item_paths: list[Path] = []
        for item_file in ITEM_REGISTRY_FILES:
            p = find_case_insensitive_file(version_dir, item_file)
            if p and p.exists() and p not in item_paths:
                item_paths.append(p)

        items_dir = version_dir / "items"
        if items_dir.is_dir():
            for item_file in ITEM_REGISTRY_FILES:
                p = find_case_insensitive_file(items_dir, item_file)
                if p and p.exists() and p not in item_paths:
                    item_paths.append(p)

        for item_path in item_paths:
            try:
                rel_item = str(item_path.relative_to(version_dir)).replace("\\", "/")
            except ValueError:
                rel_item = item_path.name
            root_tag, elements, item_messages = parse_top_level_elements(item_path)
            if item_messages:
                issues.append(FileIssue(path=rel_item, messages=item_messages))
                if log_cb:
                    for msg in item_messages:
                        log_cb("WARN", f"[{version_dir.name}] {rel_item}: {msg}")
            for element in elements:
                if element.tag == "item":
                    items.append(
                        TopLevelNode(
                            source_file=rel_item,
                            root_tag=root_tag,
                            tag=element.tag,
                            sequence=len(items),
                            element=element,
                        )
                    )

        # Ingest Creatures
        creatures: list[TopLevelNode] = []
        creatures_path = find_case_insensitive_file(version_dir, "creatures.xml")
        if not creatures_path and (version_dir / "creatures").is_dir():
            creatures_path = find_case_insensitive_file(version_dir / "creatures", "creatures.xml")

        if creatures_path and creatures_path.exists():
            try:
                rel_c = str(creatures_path.relative_to(version_dir)).replace("\\", "/")
            except ValueError:
                rel_c = creatures_path.name
            root_tag, elements, creature_messages = parse_top_level_elements(creatures_path)
            if creature_messages:
                issues.append(FileIssue(path=rel_c, messages=creature_messages))
                if log_cb:
                    for msg in creature_messages:
                        log_cb("WARN", f"[{version_dir.name}] {rel_c}: {msg}")
            for element in elements:
                if element.tag == "creature":
                    creatures.append(
                        TopLevelNode(
                            source_file=rel_c,
                            root_tag=root_tag,
                            tag=element.tag,
                            sequence=len(creatures),
                            element=element,
                        )
                    )

        # If no standard creatures.xml was found, check for partitioned monsters.xml and npcs.xml
        if not creatures:
            creatures_dir = version_dir / "creatures" if (version_dir / "creatures").is_dir() else version_dir
            monsters_path = find_case_insensitive_file(creatures_dir, "monsters.xml") or find_case_insensitive_file(version_dir, "monsters.xml")
            npcs_path = find_case_insensitive_file(creatures_dir, "npcs.xml") or find_case_insensitive_file(version_dir, "npcs.xml")

            if monsters_path and monsters_path.exists():
                try:
                    rel_m = str(monsters_path.relative_to(version_dir)).replace("\\", "/")
                except ValueError:
                    rel_m = monsters_path.name
                root_tag, elements, m_messages = parse_top_level_elements(monsters_path)
                if m_messages:
                    issues.append(FileIssue(path=rel_m, messages=m_messages))
                    if log_cb:
                        for msg in m_messages:
                            log_cb("WARN", f"[{version_dir.name}] {rel_m}: {msg}")
                for element in elements:
                    if element.tag in ("monster", "creature"):
                        c_elem = ET.Element("creature")
                        for k, v in element.attrib.items():
                            k_lower = k.lower()
                            if k_lower in ("lookaddon", "lookaddons"):
                                c_elem.attrib["lookaddons"] = v
                            else:
                                c_elem.attrib[k_lower] = v
                        if "type" not in c_elem.attrib:
                            c_elem.attrib["type"] = "monster"
                        creatures.append(
                            TopLevelNode(
                                source_file=rel_m,
                                root_tag=root_tag,
                                tag="creature",
                                sequence=len(creatures),
                                element=c_elem,
                            )
                        )

            if npcs_path and npcs_path.exists():
                try:
                    rel_n = str(npcs_path.relative_to(version_dir)).replace("\\", "/")
                except ValueError:
                    rel_n = npcs_path.name
                root_tag, elements, n_messages = parse_top_level_elements(npcs_path)
                if n_messages:
                    issues.append(FileIssue(path=rel_n, messages=n_messages))
                    if log_cb:
                        for msg in n_messages:
                            log_cb("WARN", f"[{version_dir.name}] {rel_n}: {msg}")
                for element in elements:
                    if element.tag in ("npc", "creature"):
                        c_elem = ET.Element("creature")
                        for k, v in element.attrib.items():
                            k_lower = k.lower()
                            if k_lower in ("lookaddon", "lookaddons"):
                                c_elem.attrib["lookaddons"] = v
                            else:
                                c_elem.attrib[k_lower] = v
                        if "type" not in c_elem.attrib:
                            c_elem.attrib["type"] = "npc"
                        creatures.append(
                            TopLevelNode(
                                source_file=rel_n,
                                root_tag=root_tag,
                                tag="creature",
                                sequence=len(creatures),
                                element=c_elem,
                            )
                        )

        # Ingest items.otb
        items_otb = find_case_insensitive_file(version_dir, "items.otb")
        if not items_otb or not items_otb.exists():
            for sub in ("items", "materials"):
                subdir = version_dir / sub
                if subdir.is_dir():
                    candidate = find_case_insensitive_file(subdir, "items.otb")
                    if candidate and candidate.exists():
                        items_otb = candidate
                        break

        if log_cb:
            log_cb(
                "INFO",
                f"[{version_dir.name}] Parsed: {len(borders)} borders, {len(brushes)} brushes, "
                f"{len(tilesets)} tilesets, {len(items)} items, {len(creatures)} creatures",
            )
        return LegacyVersionData(
            version=version_dir.name,
            version_dir=version_dir,
            include_files=include_files,
            metaitems=metaitems,
            borders=borders,
            brushes=brushes,
            tilesets=tilesets,
            items=items,
            creatures=creatures,
            items_otb=items_otb if (items_otb and items_otb.exists()) else None,
            issues=issues,
        )


class Normalizer:
    def normalize(
        self,
        legacy: LegacyVersionData,
        log_cb: Callable[[str, str], None] | None = None,
    ) -> NormalizedVersionData:
        palette_includes: dict[str, list[str]] = {palette: [] for palette in PALETTE_ORDER}
        tilesets: list[ModularTileset] = []
        assigned_raw_item_ids: set[int] = set()
        assigned_creature_names: set[str] = set()
        path_counts: Counter[str] = Counter()

        for node in legacy.tilesets:
            tileset_name = node.element.attrib.get("name", "Unnamed")
            for wrapper_index, wrapper in enumerate(list(node.element)):
                wrapper_tag = wrapper.tag
                file_path = self._tileset_path(tileset_name, wrapper_tag, path_counts)
                entry = ModularTileset(
                    name=tileset_name,
                    wrapper=wrapper_tag,
                    file_path=file_path,
                    source_file=node.source_file,
                    sequence=node.sequence * 10 + wrapper_index,
                    children=[clone_element(child) for child in list(wrapper)],
                )
                tilesets.append(entry)

                for palette in entry.palettes:
                    palette_includes[palette].append(file_path)

                for child in wrapper:
                    if child.tag == "item":
                        assigned_raw_item_ids.update(expand_item_ids(child))
                    elif child.tag == "creature" and "name" in child.attrib:
                        assigned_creature_names.add(child.attrib["name"])

        creature_registry = [clone_element(node.element) for node in legacy.creatures if node.tag == "creature"]
        unassigned_npcs: list[str] = []
        unassigned_other_creatures: list[str] = []
        seen_npcs: set[str] = set()
        seen_others: set[str] = set()

        for element in creature_registry:
            name = element.attrib.get("name")
            if not name or name in assigned_creature_names:
                continue
            if element.attrib.get("type", "").lower() == "npc":
                if name not in seen_npcs:
                    seen_npcs.add(name)
                    unassigned_npcs.append(name)
            else:
                if name not in seen_others:
                    seen_others.add(name)
                    unassigned_other_creatures.append(name)

        if unassigned_npcs:
            if log_cb:
                log_cb("INFO", f"[{legacy.version}] Created fallback tileset 'NPCs' for {len(unassigned_npcs)} NPC(s)")
            npc_file = self._tileset_path("NPCs", "creatures", path_counts)
            tilesets.append(
                ModularTileset(
                    name="NPCs",
                    wrapper="creatures",
                    file_path=npc_file,
                    source_file="generated",
                    sequence=max((tileset.sequence for tileset in tilesets), default=0) + 1,
                    children=[ET.Element("creature", {"name": name}) for name in unassigned_npcs],
                )
            )
            palette_includes["Creature"].append(npc_file)

        if unassigned_other_creatures:
            if log_cb:
                log_cb("INFO", f"[{legacy.version}] Created fallback tileset 'Others' for {len(unassigned_other_creatures)} creature(s)")
            others_creature_file = self._tileset_path("Others", "creatures", path_counts)
            tilesets.append(
                ModularTileset(
                    name="Others",
                    wrapper="creatures",
                    file_path=others_creature_file,
                    source_file="generated",
                    sequence=max((tileset.sequence for tileset in tilesets), default=0) + 1,
                    children=[ET.Element("creature", {"name": name}) for name in unassigned_other_creatures],
                )
            )
            palette_includes["Creature"].append(others_creature_file)

        registry_item_elements = [clone_element(node.element) for node in legacy.items if node.tag == "item"]
        registry_item_ids = item_coverage(registry_item_elements)
        metaitem_ids = set(legacy.metaitems)
        unassigned_raw_ids = sorted(registry_item_ids - assigned_raw_item_ids - metaitem_ids)
        if unassigned_raw_ids:
            if log_cb:
                log_cb("INFO", f"[{legacy.version}] Created fallback tileset 'Others' for {len(unassigned_raw_ids)} unassigned raw item(s)")
            others_raw_file = self._tileset_path("Others", "raw", path_counts)
            tilesets.append(
                ModularTileset(
                    name="Others",
                    wrapper="raw",
                    file_path=others_raw_file,
                    source_file="generated",
                    sequence=max((tileset.sequence for tileset in tilesets), default=0) + 1,
                    children=compact_item_elements(unassigned_raw_ids),
                )
            )
            palette_includes["Raw"].append(others_raw_file)

        validation = self._build_validation(
            legacy=legacy,
            tilesets=tilesets,
            palette_includes=palette_includes,
            assigned_raw_item_ids=assigned_raw_item_ids,
            assigned_creature_names=assigned_creature_names,
            registry_item_ids=registry_item_ids,
        )

        return NormalizedVersionData(
            version=legacy.version,
            metaitems=list(legacy.metaitems),
            borders=[clone_element(node.element) for node in legacy.borders if node.tag == "border"],
            brushes=[clone_element(node.element) for node in legacy.brushes if node.tag == "brush"],
            items=registry_item_elements,
            creatures=creature_registry,
            tilesets=sorted(tilesets, key=lambda tileset: (tileset.sequence, tileset.file_path)),
            palette_includes=palette_includes,
            issues=list(legacy.issues),
            validation=validation,
            items_otb=legacy.items_otb,
        )

    def _tileset_path(self, name: str, wrapper: str, path_counts: Counter[str]) -> str:
        stem = encode_filename_component(name)
        relative = f"tilesets/{wrapper}/{stem}.xml"
        path_counts[relative] += 1
        if path_counts[relative] == 1:
            return relative
        return f"tilesets/{wrapper}/{stem}_{path_counts[relative]}.xml"

    def _build_validation(
        self,
        legacy: LegacyVersionData,
        tilesets: list[ModularTileset],
        palette_includes: dict[str, list[str]],
        assigned_raw_item_ids: set[int],
        assigned_creature_names: set[str],
        registry_item_ids: set[int],
    ) -> dict[str, object]:
        legacy_tileset_wrappers: Counter[str] = Counter()
        for node in legacy.tilesets:
            for wrapper in list(node.element):
                legacy_tileset_wrappers[wrapper.tag] += 1

        normalized_tileset_wrappers = Counter(tileset.wrapper for tileset in tilesets)
        explicit_creature_tilesets = legacy_tileset_wrappers.get("creatures", 0)
        validation_messages: list[str] = []
        if explicit_creature_tilesets == 0:
            validation_messages.append("created fallback creature tilesets because the legacy data had no explicit creature palette")
        if legacy.items_otb is None:
            validation_messages.append("items.otb missing in source version")

        brush_names = {node.element.attrib.get("name", "") for node in legacy.brushes}
        border_ids = {node.element.attrib.get("id", "") for node in legacy.borders}
        creature_names = {node.element.attrib.get("name", "") for node in legacy.creatures}

        unresolved = {
            "tileset_brush_references": [],
            "tileset_creature_references": [],
            "tileset_item_references": [],
            "brush_border_references": [],
            "brush_name_references": [],
        }

        for tileset in tilesets:
            for child in tileset.children:
                if child.tag == "brush":
                    name = child.attrib.get("name", "")
                    if name and name not in brush_names:
                        unresolved["tileset_brush_references"].append({"tileset": tileset.name, "brush": name})
                elif child.tag == "creature":
                    name = child.attrib.get("name", "")
                    if name and name not in creature_names:
                        unresolved["tileset_creature_references"].append({"tileset": tileset.name, "creature": name})
                elif child.tag == "item":
                    missing = [item_id for item_id in expand_item_ids(child) if item_id not in registry_item_ids]
                    if missing:
                        unresolved["tileset_item_references"].append({"tileset": tileset.name, "missing_ids": missing[:20]})

        for node in legacy.brushes:
            brush = node.element
            for descendant in brush.iter():
                if descendant.tag in {"border", "match_border", "replace_border"}:
                    border_id = descendant.attrib.get("id")
                    if border_id and border_id not in border_ids:
                        unresolved["brush_border_references"].append(
                            {"brush": brush.attrib.get("name", ""), "border_id": border_id, "tag": descendant.tag}
                        )
                if descendant.tag in {"friend", "enemy"}:
                    name = descendant.attrib.get("name", "")
                    if name and name not in brush_names and name not in {"all", "none"}:
                        unresolved["brush_name_references"].append(
                            {"brush": brush.attrib.get("name", ""), "reference": name, "tag": descendant.tag}
                        )
                if descendant.tag == "border":
                    target = descendant.attrib.get("to")
                    if target and target not in {"all", "none"} and target not in brush_names:
                        unresolved["brush_name_references"].append(
                            {"brush": brush.attrib.get("name", ""), "reference": target, "tag": "border.to"}
                        )

        return {
            "legacy_counts": {
                "borders": len(legacy.borders),
                "brushes": len(legacy.brushes),
                "metaitems": len(legacy.metaitems),
                "creatures": len(legacy.creatures),
                "items": len(legacy.items),
                "item_coverage": len(registry_item_ids),
                "tilesets_by_wrapper": dict(legacy_tileset_wrappers),
            },
            "normalized_counts": {
                "borders": len(legacy.borders),
                "brushes": len(legacy.brushes),
                "metaitems": len(legacy.metaitems),
                "creatures": len(legacy.creatures),
                "items": len(legacy.items),
                "item_coverage": len(registry_item_ids),
                "tilesets_by_wrapper": dict(normalized_tileset_wrappers),
                "palettes": {palette: len(paths) for palette, paths in palette_includes.items()},
                "assigned_raw_items": len(assigned_raw_item_ids),
                "assigned_creatures": len(assigned_creature_names),
            },
            "messages": validation_messages,
            "unresolved": unresolved,
        }


class ModularWriter:
    def __init__(self, output_root: Path, target_folder_name: str | None = None) -> None:
        self.output_root = output_root
        self.target_folder_name = target_folder_name

    def write(
        self,
        version: NormalizedVersionData,
        progress_cb: Callable[[int, int, str], None] | None = None,
        log_cb: Callable[[str, str], None] | None = None,
    ) -> Path:
        folder_name = version.version
        if self.target_folder_name and is_valid_folder_name(self.target_folder_name):
            folder_name = self.target_folder_name.strip()
        version_dir = self.output_root / folder_name
        if log_cb:
            log_cb("INFO", f"[{version.version}] Output destination: {version_dir}")
        if version_dir.exists():
            shutil.rmtree(version_dir)
        version_dir.mkdir(parents=True, exist_ok=True)

        for folder in ("borders", "brushes", "creatures", "items", "tilesets"):
            (version_dir / folder).mkdir(parents=True, exist_ok=True)

        total_steps = 7 + len(version.tilesets) + (1 if version.items_otb and version.items_otb.exists() else 0) + 1
        current_step = 0

        def advance(status: str) -> None:
            nonlocal current_step
            current_step += 1
            if progress_cb:
                progress_cb(current_step, total_steps, status)

        advance(f"[{version.version}] Writing materials.xml")
        self._write_materials(version_dir, version.metaitems)

        advance(f"[{version.version}] Writing palettes.xml")
        self._write_palettes(version_dir, version.palette_includes)

        advance(f"[{version.version}] Writing borders/borders.xml")
        self._write_border_module(version_dir, version.borders)

        advance(f"[{version.version}] Writing brushes/brushes.xml")
        self._write_brush_module(version_dir, version.brushes)

        advance(f"[{version.version}] Writing creatures/creatures.xml")
        self._write_creature_module(version_dir, version.creatures)

        advance(f"[{version.version}] Writing items/items.xml")
        self._write_item_module(version_dir, version.items)

        for tileset in version.tilesets:
            advance(f"[{version.version}] Writing {tileset.file_path}")
            self._write_single_tileset(version_dir, tileset)

        if version.items_otb is not None and version.items_otb.exists():
            advance(f"[{version.version}] Copying items.otb")
            shutil.copy2(version.items_otb, version_dir / "items.otb")

        advance(f"[{version.version}] Smoke-testing generated XML files...")
        smoke_result = self._smoke_test(version_dir, log_cb=log_cb)

        report = {
            "version": version.version,
            "issues": [{"path": issue.path, "messages": issue.messages} for issue in version.issues],
            "validation": version.validation,
            "generated_files": self._generated_file_list(version_dir),
            "smoke_test": smoke_result,
        }
        (version_dir / "conversion_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        if log_cb:
            log_cb("SUCCESS", f"[{version.version}] Finished conversion -> {version_dir} ({len(report['generated_files'])} files generated)")
        return version_dir

    def _write_materials(self, version_dir: Path, metaitems: list[int]) -> None:
        root = ET.Element("materials")
        for metaitem in metaitems:
            ET.SubElement(root, "metaitem", {"id": str(metaitem)})

        for section_name, folder, recursive in MODULE_SECTIONS:
            section = ET.SubElement(root, section_name)
            include_attrib = {"folder": folder}
            if recursive:
                include_attrib["subfolders"] = "true"
            ET.SubElement(section, "include", include_attrib)

        palettes = ET.SubElement(root, "palettes")
        ET.SubElement(palettes, "include", {"file": "palettes.xml"})
        write_xml(version_dir / "materials.xml", root)

    def _write_palettes(self, version_dir: Path, palette_includes: dict[str, list[str]]) -> None:
        root = ET.Element("palettes")
        for palette_name in PALETTE_ORDER:
            palette = ET.SubElement(root, "palette", {"name": palette_name})
            tileset = ET.SubElement(palette, "tileset")
            for file_path in palette_includes[palette_name]:
                ET.SubElement(tileset, "include", {"file": file_path.replace("\\", "/")})
        write_xml(version_dir / "palettes.xml", root)

    def _write_border_module(self, version_dir: Path, borders: list[ET.Element]) -> None:
        root = ET.Element("materials")
        for border in borders:
            root.append(clone_element(border))
        write_xml(version_dir / "borders" / "borders.xml", root)

    def _write_brush_module(self, version_dir: Path, brushes: list[ET.Element]) -> None:
        root = ET.Element("brushes")
        for brush in brushes:
            root.append(clone_element(brush))
        write_xml(version_dir / "brushes" / "brushes.xml", root)

    def _write_creature_module(self, version_dir: Path, creatures: list[ET.Element]) -> None:
        root = ET.Element("creatures")
        for creature in creatures:
            root.append(clone_element(creature))
        write_xml(version_dir / "creatures" / "creatures.xml", root)

    def _write_item_module(self, version_dir: Path, items: list[ET.Element]) -> None:
        root = ET.Element("items")
        for item in items:
            root.append(clone_element(item))
        write_xml(version_dir / "items" / "items.xml", root)

    def _write_single_tileset(self, version_dir: Path, tileset: ModularTileset) -> None:
        path = version_dir / tileset.file_path
        path.parent.mkdir(parents=True, exist_ok=True)
        root = ET.Element("tileset", {"name": tileset.name})
        for child in tileset.children:
            root.append(clone_element(child))
        write_xml(path, root)

    def _write_tilesets(self, version_dir: Path, tilesets: list[ModularTileset]) -> None:
        for tileset in tilesets:
            self._write_single_tileset(version_dir, tileset)

    def _generated_file_list(self, version_dir: Path) -> list[str]:
        return sorted(str(path.relative_to(version_dir)).replace("\\", "/") for path in version_dir.rglob("*") if path.is_file())

    def _smoke_test(
        self,
        version_dir: Path,
        log_cb: Callable[[str, str], None] | None = None,
    ) -> dict[str, object]:
        parsed_files: list[str] = []
        failures: list[str] = []
        for xml_path in sorted(version_dir.rglob("*.xml")):
            try:
                parse_root(xml_path)
                parsed_files.append(str(xml_path.relative_to(version_dir)).replace("\\", "/"))
            except Exception as exc:
                failures.append(f"{xml_path.name}: {exc}")
                if log_cb:
                    log_cb("ERROR", f"[{version_dir.name}] Smoke test failed for {xml_path.name}: {exc}")
        if not failures and log_cb:
            log_cb("INFO", f"[{version_dir.name}] Smoke test passed: {len(parsed_files)} XML file(s) verified.")
        return {"parsed_files": parsed_files, "failures": failures}


def summarize_issues(issues: list[FileIssue]) -> list[str]:
    summary: list[str] = []
    for issue in issues:
        for message in issue.messages:
            summary.append(f"{issue.path}: {message}")
    return summary


def inspect_source_path(path: Path) -> dict[str, object]:
    resolved_path = resolve_version_root(path)
    if not resolved_path.exists() or not resolved_path.is_dir():
        return {
            "is_valid": False,
            "is_single_version": False,
            "versions": [],
            "version_dirs": {},
            "resolved_path": str(resolved_path),
            "error": f"Path does not exist or is not a directory: {path}",
        }

    reader = LegacyVersionReader(resolved_path)
    versions = reader.discover_versions()
    if not versions:
        return {
            "is_valid": False,
            "is_single_version": False,
            "versions": [],
            "version_dirs": {},
            "resolved_path": str(resolved_path),
            "error": "No legacy client folders found (no materials.xml detected in path or subdirectories).",
        }

    is_single = is_version_dir(resolved_path)
    v_dict = {v.name: v for v in versions}
    return {
        "is_valid": True,
        "is_single_version": is_single,
        "versions": [v.name for v in versions],
        "version_dirs": v_dict,
        "version_paths": v_dict,
        "resolved_path": str(resolved_path),
        "error": None,
    }


def inspect_legacy_details(version_dir: Path, log_cb: Callable[[str, str], None] | None = None) -> dict[str, object]:
    version_dir = resolve_version_root(version_dir)
    reader = LegacyVersionReader(version_dir.parent if version_dir.parent.is_dir() else version_dir)
    legacy = reader.read(version_dir, log_cb=log_cb)

    files = []
    for p in sorted(version_dir.rglob("*")):
        if p.is_file():
            rel_name = str(p.relative_to(version_dir)).replace("\\", "/")
            files.append({
                "name": rel_name,
                "size": p.stat().st_size,
            })

    is_partitioned = any("/" in f["name"] for f in files)

    return {
        "version": legacy.version,
        "version_dir": str(version_dir),
        "structure_layout": "Partitioned (Crystal / Forked Layout)" if is_partitioned else "Flat (Standard Legacy Layout)",
        "files": files,
        "include_files": legacy.include_files,
        "metaitems": legacy.metaitems,
        "borders_count": len(legacy.borders),
        "brushes_count": len(legacy.brushes),
        "tilesets_count": len(legacy.tilesets),
        "items_count": len(legacy.items),
        "creatures_count": len(legacy.creatures),
        "has_items_otb": legacy.items_otb is not None and legacy.items_otb.exists(),
        "issues": [{"path": issue.path, "messages": issue.messages} for issue in legacy.issues],
    }


def preview_normalized(version_dir: Path, log_cb: Callable[[str, str], None] | None = None) -> dict[str, object]:
    version_dir = resolve_version_root(version_dir)
    reader = LegacyVersionReader(version_dir.parent if version_dir.parent.is_dir() else version_dir)
    legacy = reader.read(version_dir, log_cb=log_cb)
    normalizer = Normalizer()
    normalized = normalizer.normalize(legacy, log_cb=log_cb)

    expected_files = [
        "materials.xml",
        "palettes.xml",
        "borders/borders.xml",
        "brushes/brushes.xml",
        "creatures/creatures.xml",
        "items/items.xml",
    ]
    if normalized.items_otb:
        expected_files.append("items.otb")
    expected_files.append("conversion_report.json")
    for t in normalized.tilesets:
        expected_files.append(t.file_path)

    expected_files.sort()
    return {
        "version": normalized.version,
        "expected_files": expected_files,
        "tilesets_count": len(normalized.tilesets),
        "tilesets_by_wrapper": dict(Counter(t.wrapper for t in normalized.tilesets)),
        "palettes": {p: len(paths) for p, paths in normalized.palette_includes.items()},
        "validation": normalized.validation,
        "issues": [{"path": issue.path, "messages": issue.messages} for issue in normalized.issues],
    }


def run_conversion(
    base_dir: Path,
    output_root: Path,
    versions: list[str] | None = None,
    target_folder_name: str | None = None,
    log_cb: Callable[[str, str], None] | None = None,
    progress_cb: Callable[[int, int, str], None] | None = None,
) -> list[tuple[str, Path, list[str]]]:
    reader = LegacyVersionReader(base_dir)
    normalizer = Normalizer()
    selected = reader.discover_versions()
    if versions:
        version_set = set(versions)
        selected = [path for path in selected if path.name in version_set]

    if not selected:
        raise RuntimeError(f"No version folders found to convert in {base_dir}")

    results: list[tuple[str, Path, list[str]]] = []
    total_versions = len(selected)

    for idx, version_dir in enumerate(selected):
        if log_cb:
            log_cb("INFO", f"=== Starting conversion ({idx + 1}/{total_versions}): {version_dir.name} ===")

        folder_name = target_folder_name if (total_versions == 1 and target_folder_name and is_valid_folder_name(target_folder_name)) else None
        writer = ModularWriter(output_root, target_folder_name=folder_name)

        legacy = reader.read(version_dir, log_cb=log_cb)
        normalized = normalizer.normalize(legacy, log_cb=log_cb)
        written_dir = writer.write(normalized, progress_cb=progress_cb, log_cb=log_cb)
        results.append((version_dir.name, written_dir, summarize_issues(normalized.issues)))

        if log_cb:
            log_cb("SUCCESS", f"=== Completed version {version_dir.name} -> {written_dir} ===")

    return results


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert legacy RME data folders into modular XML output.")
    parser.add_argument(
        "--base-dir",
        type=Path,
        default=None,
        help="Directory that contains numeric version folders or a single client version folder.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Output directory for modularized client data.",
    )
    parser.add_argument(
        "--output-name",
        type=str,
        default=None,
        help="Optional custom output folder name (when converting a single version).",
    )
    parser.add_argument(
        "--versions",
        nargs="*",
        default=None,
        help="Optional list of version folders to convert. Defaults to all numeric folders.",
    )
    parser.add_argument(
        "--gui",
        action="store_true",
        help="Launch the wxPython graphical user interface.",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.gui or len(argv) == 0:
        try:
            import wx  # noqa: F401
            from convert_legacy_data_gui import launch_gui
            return launch_gui(initial_base_dir=args.base_dir, initial_output_dir=args.output_dir)
        except ImportError:
            if args.gui:
                print("Error: wxPython is not installed. Run 'pip install wxPython' to use the GUI.", file=sys.stderr)
                return 1

    if not args.base_dir:
        print("Error: --base-dir is required in CLI mode (or run with --gui to open the GUI).", file=sys.stderr)
        return 1

    output_dir = args.output_dir or (args.base_dir / "new_data")
    try:
        results = run_conversion(
            args.base_dir,
            output_dir,
            args.versions,
            target_folder_name=args.output_name,
            log_cb=lambda level, msg: print(f"[{level}] {msg}"),
        )
    except Exception as exc:
        print(f"Conversion failed: {exc}", file=sys.stderr)
        return 1

    print(f"Converted {len(results)} version folder(s) into {output_dir}")
    for version, written_dir, issues in results:
        print(f"- {version}: {written_dir}")
        if issues:
            print(f"  issues: {len(issues)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
