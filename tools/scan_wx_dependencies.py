#!/usr/bin/env python3
"""
scan_wx_dependencies.py - Audit and verify wxWidgets decoupling across RME codebase.

Enforces zero wxWidgets dependencies in core domain targets (rme_core) and
tracks wxWidgets footprint across all subsystems for future decoupling (e.g. Qt6 / Headless).

Zero-dependency Python 3.9+ script.
Usage:
    python tools/scan_wx_dependencies.py [--target core|all|rme_core] [--format text|md|json] [--strict]
"""

import os
import sys
import re
import json
import argparse
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, NamedTuple, Optional, Set, Tuple

# Patterns
RE_WX_INCLUDE = re.compile(r'^\s*#\s*include\s*[<"](wx/|wxWidgets/|.*wx\.h).*')
RE_WX_TYPE = re.compile(
    r'\b(wxString|wxPoint|wxRect|wxSize|wxArrayString|wxDateTime|wxFileName|'
    r'wxColour|wxColor|wxWindow|wxDialog|wxFrame|wxPanel|wxBitmap|wxImage|'
    r'wxDC|wxFont|wxCoord|wxObject|wxEvtHandler|wxCommandEvent|wxKeyEvent|'
    r'wxMouseEvent|wxThread|wxMutex|wxCriticalSection|wxFile|wxDir|'
    r'wxTimer|wxTimerEvent|wxScrolledWindow|wxAuiManager|wxAuiPaneInfo)\b'
)
RE_WX_MACRO = re.compile(r'\b(wxstr|wxT|wxEmptyString|wxNOT_FOUND|wxID_[A-Z0-9_]+|wxTRANSLATE)\b')
RE_GUI_LEAK = re.compile(r'^\s*#\s*include\s*["<]ui/|\b(g_gui|DialogUtil::|g_status\.)')
RE_MAIN_H_LEAK = re.compile(r'^\s*#\s*include\s*["<]app/main\.h[">]')

class Violation(NamedTuple):
    file_path: str
    line_num: int
    rule_type: str  # WX_INCLUDE, WX_TYPE, WX_MACRO, GUI_LEAK, MAIN_H_LEAK
    code: str
    subsystem: str

def parse_cmake_core_files(cmake_path: Path) -> Set[str]:
    """Parse source/CMakeLists.txt to extract files comprising the rme_core target."""
    if not cmake_path.is_file():
        return set()
    
    text = cmake_path.read_text(encoding='utf-8', errors='ignore')
    core_files = set()
    for var in ('rme_core_H', 'rme_core_SRC'):
        pattern = rf'set\({var}\s+(.*?)\)'
        match = re.search(pattern, text, re.DOTALL)
        if match:
            for item in match.group(1).split():
                cleaned = item.strip().replace('${CMAKE_CURRENT_LIST_DIR}/', 'source/')
                if cleaned.startswith('source/'):
                    core_files.add(cleaned.replace('\\', '/'))
    return core_files

def get_subsystem(file_path: str) -> str:
    """Classify file into subsystem based on directory structure."""
    norm = file_path.replace('\\', '/')
    parts = norm.split('/')
    if len(parts) >= 2 and parts[0] == 'source':
        return parts[1]
    return 'other'

def is_domain_path(file_path: str) -> bool:
    """Identify domain/backend modules that must have zero wxWidgets dependencies."""
    norm = file_path.replace('\\', '/')
    domain_prefixes = (
        'source/brushes/',
        'source/game/',
        'source/io/',
        'source/item_definitions/',
        'source/map/',
        'source/editor/operations/',
        'source/editor/persistence/',
    )
    if any(norm.startswith(p) for p in domain_prefixes):
        # Exclude known UI helper files if located under domain
        if norm.startswith('source/brushes/managers/autoborder_preview_manager') or \
           norm.startswith('source/brushes/managers/brush_manager') or \
           norm.startswith('source/editor/operations/search_query') or \
           norm.startswith('source/editor/operations/collection_extractor_service'):
            return False
        return True
    
    if norm in ('source/util/file_system.cpp', 'source/util/file_system.h',
                'source/util/common.cpp', 'source/util/common.h'):
        return True

    return False

def scan_file(file_path: Path, repo_root: Path) -> List[Violation]:
    """Scan a single C++ source or header file for wxWidgets and UI leaks."""
    rel_path = str(file_path.relative_to(repo_root)).replace('\\', '/')
    subsystem = get_subsystem(rel_path)
    is_domain = is_domain_path(rel_path)
    violations: List[Violation] = []

    try:
        lines = file_path.read_text(encoding='utf-8', errors='ignore').splitlines()
    except Exception:
        return violations

    in_multiline_comment = False
    for idx, line in enumerate(lines, 1):
        sline = line.strip()
        if not sline:
            continue

        if '/*' in sline and '*/' in sline:
            # Inline comment, remove comment part for check
            sline = re.sub(r'/\*.*?\*/', '', sline).strip()
        elif '/*' in sline:
            in_multiline_comment = True
            continue
        elif '*/' in sline:
            in_multiline_comment = False
            continue
        elif in_multiline_comment:
            continue

        if sline.startswith('//'):
            continue

        if RE_WX_INCLUDE.search(sline):
            violations.append(Violation(rel_path, idx, 'WX_INCLUDE', sline, subsystem))
        if RE_WX_TYPE.search(sline):
            violations.append(Violation(rel_path, idx, 'WX_TYPE', sline, subsystem))
        if RE_WX_MACRO.search(sline):
            violations.append(Violation(rel_path, idx, 'WX_MACRO', sline, subsystem))
        if RE_GUI_LEAK.search(sline):
            violations.append(Violation(rel_path, idx, 'GUI_LEAK', sline, subsystem))
        if is_domain and RE_MAIN_H_LEAK.search(sline):
            violations.append(Violation(rel_path, idx, 'MAIN_H_LEAK', sline, subsystem))

    return violations

def run_scan(repo_root: Path, target_mode: str) -> Tuple[List[Violation], Dict[str, Dict[str, int]], int]:
    """Execute scan across selected target profile."""
    cmake_path = repo_root / 'source' / 'CMakeLists.txt'
    core_files = parse_cmake_core_files(cmake_path)
    
    files_to_scan: List[Path] = []
    source_dir = repo_root / 'source'

    for root, dirs, files in os.walk(source_dir):
        dirs[:] = [d for d in dirs if d not in ('.git', 'build', 'build-ninja', 'vcpkg_installed', '__pycache__')]
        for f in files:
            if f.endswith(('.cpp', '.h', '.hpp')):
                p = Path(root) / f
                rel = str(p.relative_to(repo_root)).replace('\\', '/')
                if target_mode == 'rme_core':
                    if rel in core_files:
                        files_to_scan.append(p)
                elif target_mode == 'core':
                    if is_domain_path(rel) or rel in core_files:
                        files_to_scan.append(p)
                else:  # 'all'
                    files_to_scan.append(p)

    all_violations: List[Violation] = []
    subsystem_stats = defaultdict(lambda: {'files': 0, 'WX_INCLUDE': 0, 'WX_TYPE': 0, 'WX_MACRO': 0, 'GUI_LEAK': 0, 'MAIN_H_LEAK': 0})

    for p in files_to_scan:
        rel = str(p.relative_to(repo_root)).replace('\\', '/')
        sub = get_subsystem(rel)
        subsystem_stats[sub]['files'] += 1
        v_list = scan_file(p, repo_root)
        for v in v_list:
            subsystem_stats[sub][v.rule_type] += 1
            all_violations.append(v)

    return all_violations, subsystem_stats, len(files_to_scan)

def format_text(violations: List[Violation], stats: Dict[str, Dict[str, int]], total_files: int, target_mode: str, summary_only: bool) -> str:
    lines = []
    lines.append("=" * 80)
    lines.append(f"  RME Redux - wxWidgets Decoupling Audit Report (Target: {target_mode})")
    lines.append("=" * 80)
    lines.append(f"Total files audited: {total_files}")
    lines.append(f"Total violations found: {len(violations)}\n")

    lines.append("Subsystem Breakdown:")
    lines.append(f"{'Subsystem':<20} {'Files':<6} {'Includes':<10} {'Types':<8} {'Macros':<8} {'GUI Leaks':<10} {'Total':<6}")
    lines.append("-" * 72)
    for sub, s in sorted(stats.items()):
        tot = s['WX_INCLUDE'] + s['WX_TYPE'] + s['WX_MACRO'] + s['GUI_LEAK'] + s['MAIN_H_LEAK']
        lines.append(f"{sub:<20} {s['files']:<6} {s['WX_INCLUDE']:<10} {s['WX_TYPE']:<8} {s['WX_MACRO']:<8} {s['GUI_LEAK'] + s['MAIN_H_LEAK']:<10} {tot:<6}")

    if not summary_only and violations:
        lines.append("\nDetailed Violations:")
        lines.append("-" * 80)
        for v in violations:
            lines.append(f"{v.file_path}:{v.line_num} [{v.rule_type}] {v.code}")

    lines.append("=" * 80)
    return "\n".join(lines)

def format_markdown(violations: List[Violation], stats: Dict[str, Dict[str, int]], total_files: int, target_mode: str, summary_only: bool) -> str:
    lines = []
    lines.append(f"# wxWidgets Decoupling Audit Report (`{target_mode}`)")
    lines.append(f"\n- **Total files audited**: {total_files}")
    lines.append(f"- **Total violations**: {len(violations)}\n")
    lines.append("### Subsystem Breakdown\n")
    lines.append("| Subsystem | Files | WX Includes | WX Types | WX Macros | GUI Leaks | Total |")
    lines.append("|:---|---:|---:|---:|---:|---:|---:|")
    for sub, s in sorted(stats.items()):
        tot = s['WX_INCLUDE'] + s['WX_TYPE'] + s['WX_MACRO'] + s['GUI_LEAK'] + s['MAIN_H_LEAK']
        lines.append(f"| `{sub}` | {s['files']} | {s['WX_INCLUDE']} | {s['WX_TYPE']} | {s['WX_MACRO']} | {s['GUI_LEAK'] + s['MAIN_H_LEAK']} | **{tot}** |")

    if not summary_only and violations:
        lines.append("\n### Detailed Violations\n")
        lines.append("| File | Line | Rule | Snippet |")
        lines.append("|:---|---:|:---|:---|")
        for v in violations:
            clean_code = v.code.replace('|', '\\|')
            lines.append(f"| `{v.file_path}` | {v.line_num} | `{v.rule_type}` | `{clean_code}` |")
    return "\n".join(lines)

def format_json(violations: List[Violation], stats: Dict[str, Dict[str, int]], total_files: int, target_mode: str) -> str:
    data = {
        "target_mode": target_mode,
        "total_files": total_files,
        "total_violations": len(violations),
        "subsystem_stats": stats,
        "violations": [
            {
                "file_path": v.file_path,
                "line_num": v.line_num,
                "rule_type": v.rule_type,
                "subsystem": v.subsystem,
                "code": v.code
            }
            for v in violations
        ]
    }
    return json.dumps(data, indent=2)

def main():
    parser = argparse.ArgumentParser(description="Audit and verify wxWidgets decoupling in RME.")
    parser.add_argument(
        '--target',
        choices=['core', 'rme_core', 'all'],
        default='core',
        help="Target scope: 'core' (domain modules), 'rme_core' (CMake rme_core target), 'all' (entire codebase)."
    )
    parser.add_argument(
        '--format',
        choices=['text', 'md', 'json'],
        default='text',
        help="Report format (default: text)."
    )
    parser.add_argument(
        '--summary',
        action='store_true',
        help="Print summary only, omit detailed line violations."
    )
    parser.add_argument(
        '--strict',
        action='store_true',
        help="Return non-zero exit code if violations are found in target."
    )
    parser.add_argument(
        '--output',
        type=str,
        help="Write output to specified file."
    )

    args = parser.parse_args()
    repo_root = Path(__file__).resolve().parent.parent

    violations, stats, total_files = run_scan(repo_root, args.target)

    if args.format == 'json':
        report = format_json(violations, stats, total_files, args.target)
    elif args.format == 'md':
        report = format_markdown(violations, stats, total_files, args.target, args.summary)
    else:
        report = format_text(violations, stats, total_files, args.target, args.summary)

    if args.output:
        Path(args.output).write_text(report, encoding='utf-8')
    else:
        print(report)

    if args.strict and violations:
        sys.exit(1)
    sys.exit(0)

if __name__ == '__main__':
    main()
