#!/usr/bin/env python3
"""
Local test suite for AI issue and PR triage with EmbeddingGemma 2.
Evaluates zero-shot semantic embedding classification against real-world RME scenarios.
"""

import sys
from pathlib import Path
from typing import Dict, List

# Add parent directory to path to import triage module
scripts_dir = Path(__file__).resolve().parent
sys.path.insert(0, str(scripts_dir))

from triage import (
    DEFAULT_LABELS,
    NON_TRIAGE_LABELS,
    PATH_TO_AREA,
    build_label_anchor,
    build_target_text,
    get_embedding_model,
    compute_embeddings,
    select_labels_from_scores,
)


TEST_SCENARIOS = [
    {
        "name": "Rendering Bug",
        "title": "Black tile artifacts when zooming past 200% on OpenGL",
        "body": "When zooming in on a dense map, the tile renderer displays black squares and flickering textures in OpenGL mode.",
        "is_pr": False,
        "changed_files": [],
        "expected_type": "Type: Bug",
        "expected_area": "Area: Rendering",
    },
    {
        "name": "Build Failure",
        "title": "Fix vcpkg toolchain build failure on Windows Ninja",
        "body": "Compilation error in build_ninja when compiling C++20 modules with MSVC on Windows x64 runner in GitHub Actions.",
        "is_pr": False,
        "changed_files": [],
        "expected_type": "Type: Bug",
        "expected_area": "Area: Build",
    },
    {
        "name": "UI Feature Request",
        "title": "Add customizable dark theme palette colors to preferences dialog",
        "body": "Feature request: It would be great to have an option in the wxWidgets preferences window to adjust toolbar and dialog themes.",
        "is_pr": False,
        "changed_files": [],
        "expected_type": "Type: Enhancement",
        "expected_area": "Area: UI",
    },
    {
        "name": "Lua Scripting Binding",
        "title": "Expose tile elevation and spatial query in Lua API",
        "body": "PR adding new bindings to lua_api_tile and lua_api_map to allow scripts to query tile height and zone flags.",
        "is_pr": True,
        "changed_files": ["source/lua/lua_api_tile.cpp", "source/lua/lua_api_tile.h"],
        "expected_type": "Type: Enhancement",
        "expected_area": "Area: Scripts",
    },
    {
        "name": "Brush Bordering Bug",
        "title": "Mountain ground brush places wrong autoborder on cliff edges",
        "body": "When painting with the mountain ground brush, autobordering places grass transition borders instead of rock cliff borders.",
        "is_pr": False,
        "changed_files": [],
        "expected_type": "Type: Bug",
        "expected_area": "Area: Brushes",
    },
    {
        "name": "Documentation Update",
        "title": "Update build instructions and prerequisites in README.md",
        "body": "Clarifies the vcpkg dependencies and MSVC requirements in README and AGENTS.md for Windows developers.",
        "is_pr": True,
        "changed_files": ["README.md", "AGENTS.md"],
        "expected_type": "Type: Documentation",
        "expected_area": None,
    },
    {
        "name": "Fluid Container Bug (PR #1081)",
        "title": "fix: resolve fluid container recognition, sprite color mapping, and click freeze",
        "body": "Bug fix: This PR resolves fluid container recognition, sprite color mapping defects, and editor freeze / crash on double-click. Fixes map save data loss in ItemSerializationOTBM.",
        "is_pr": True,
        "changed_files": [
            "source/item_definitions/core/item_definition_resolver.cpp",
            "source/io/otbm/item_serializer.cpp",
            "source/ui/properties/splash_properties_window.cpp",
        ],
        "expected_type": "Type: Bug",
        "expected_area": "Area: I/O",
    },
    {
        "name": "Statusbar UI Bug (PR #1082)",
        "title": "fix(ui): disable statusbar size grip to prevent dark mode assertion failure",
        "body": "Disables size grip on wxStatusBar to prevent assertion crash when dark mode theme is enabled.",
        "is_pr": True,
        "changed_files": ["source/ui/main_frame.cpp"],
        "expected_type": "Type: Bug",
        "expected_area": "Area: UI",
    },
    {
        "name": "Sector Map Import Feature (PR #1083)",
        "title": "feat(io): open and save original server sector maps (.sec)",
        "body": "Adds support to open and save CipSoft server sector maps (.sec) with coordinate offsets.",
        "is_pr": True,
        "changed_files": ["source/io/sec_map.cpp", "source/io/sec_map.h"],
        "expected_type": "Type: Enhancement",
        "expected_area": "Area: I/O",
    },
]


def run_tests():
    print("=" * 60)
    print("Starting Local Triage Tests with EmbeddingGemma 2")
    print("=" * 60)

    # 1. Initialize embedding backend
    backend = get_embedding_model()

    # 2. Precompute label embeddings (excluding status, priority, and non-triage labels)
    candidate_labels = [
        k for k in DEFAULT_LABELS.keys()
        if not (k.startswith("Status:") or k.startswith("Priority:") or k in NON_TRIAGE_LABELS)
    ]
    label_anchors = [build_label_anchor(k, DEFAULT_LABELS.get(k, "")) for k in candidate_labels]
    print(f"Precomputing embeddings for {len(candidate_labels)} candidate labels...")
    label_vecs = compute_embeddings(backend, label_anchors)

    passed_count = 0
    total_count = len(TEST_SCENARIOS)

    for i, test in enumerate(TEST_SCENARIOS, 1):
        print(f"\n[{i}/{total_count}] Testing: {test['name']}")
        print(f"  Title: {test['title']}")

        target_text = build_target_text(
            title=test["title"],
            body=test["body"],
            is_pr=test["is_pr"],
            changed_files=test["changed_files"],
        )
        target_vec = compute_embeddings(backend, [target_text])[0]

        # Calculate similarity scores
        scores: Dict[str, float] = {}
        for idx, lbl_name in enumerate(candidate_labels):
            scores[lbl_name] = float(target_vec @ label_vecs[idx])

        # Apply directory reinforcement if PR files match known subsystem
        if test["is_pr"] and test["changed_files"]:
            boosted_areas = set()
            for f in test["changed_files"]:
                for prefix, area in PATH_TO_AREA.items():
                    if f.startswith(prefix) and area not in boosted_areas:
                        scores[area] = scores.get(area, 0.0) + 0.03
                        boosted_areas.add(area)

        selected, _ = select_labels_from_scores(scores)
        print(f"  Selected Labels: {selected}")

        # Check expectations
        expected_type = test["expected_type"]
        expected_area = test["expected_area"]

        type_match = expected_type is None or expected_type in selected

        if expected_area is None:
            # For docs, if any area is selected, it must be related (e.g. Build), never unrelated like Rendering or Brushes
            selected_areas = [lbl for lbl in selected if lbl.startswith("Area:")]
            area_match = len(selected_areas) == 0 or all(a in ("Area: Build", "Area: Data") for a in selected_areas)
        else:
            area_match = expected_area in selected

        if type_match and area_match:
            print("  Result: PASSED")
            passed_count += 1
        else:
            print(f"  Result: FAILED (Expected Type: '{expected_type}', Area: '{expected_area}')")

    print("\n" + "=" * 60)
    print(f"Test Summary: {passed_count}/{total_count} scenarios passed.")
    print("=" * 60)

    return 0 if passed_count == total_count else 1


if __name__ == "__main__":
    sys.exit(run_tests())
