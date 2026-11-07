#!/usr/bin/env python3
"""
scan_legacy_wx.py - Scan RME codebase for all legacy wxWidgets patterns,
deprecated APIs (from wx 2.8 / 3.0 / 3.2), and modernization opportunities
for wxWidgets 3.3.x.

Zero-dependency Python 3.9+ script.
Usage:
    python tools/scan_legacy_wx.py [options]

Options:
    --dir <path>          Directory to scan (default: source)
    --format <text|md|json> Output format (default: text)
    --category <cat>      Filter by specific category
    --summary             Print summary only
    --output <file>       Write output to file
    --help                Show this message
"""

import os
import sys
import re
import json
import argparse
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional

class Finding(NamedTuple):
    file_path: str
    line_num: int
    line_content: str
    rule_id: str
    category: str
    severity: str  # ERROR, WARNING, OPPORTUNITY
    description: str
    suggestion: str

class Rule(NamedTuple):
    rule_id: str
    category: str
    severity: str
    pattern: re.Pattern
    description: str
    suggestion: str

RULES: List[Rule] = [
    # -------------------------------------------------------------------------
    # 1. DEPRECATED OR REMOVED APIS (from wx 2.8 / 3.0 / 3.2 / 3.3)
    # -------------------------------------------------------------------------
    Rule(
        rule_id="WX_DEP_EVENT_TABLE",
        category="deprecated_api",
        severity="ERROR",
        pattern=re.compile(r'\b(BEGIN_EVENT_TABLE|END_EVENT_TABLE|DECLARE_EVENT_TABLE)\b'),
        description="Static event tables (BEGIN_EVENT_TABLE) are deprecated since wx 3.0.",
        suggestion="Replace with dynamic Bind(...) calls in constructor or init method."
    ),
    Rule(
        rule_id="WX_DEP_MAKE_MODAL",
        category="deprecated_api",
        severity="ERROR",
        pattern=re.compile(r'\bMakeModal\s*\('),
        description="wxWindow::MakeModal() was deprecated in 3.0 and removed in 3.2.",
        suggestion="Use wxDialog::ShowModal() or wxWindowDisabler instead."
    ),
    Rule(
        rule_id="WX_DEP_NEW_ID",
        category="deprecated_api",
        severity="WARNING",
        pattern=re.compile(r'\b(wxNewId|NewControlId)\s*\('),
        description="wxNewId() and NewControlId() are obsolete since wx 3.0.",
        suggestion="Use wxID_ANY or wxWindow::NewControlId()."
    ),
    Rule(
        rule_id="WX_DEP_STRING_WRITE_BUF",
        category="deprecated_api",
        severity="WARNING",
        pattern=re.compile(r'\b(GetWriteBuf|UngetWriteBuf)\s*\('),
        description="wxString::GetWriteBuf() / UngetWriteBuf() are deprecated.",
        suggestion="Use wxStringBuffer or wxStringBufferLength, or std::string directly."
    ),
    Rule(
        rule_id="WX_DEP_FILENAME_LEGACY",
        category="deprecated_api",
        severity="WARNING",
        pattern=re.compile(r'\b(wxDos2UnixFilename|wxUnix2DosFilename|wxStripExtension|wxGetTempFileName|wxExpandPath|wxSplitPath)\b'),
        description="Global filename utility functions are deprecated in favor of wxFileName / std::filesystem.",
        suggestion="Use std::filesystem::path or wxFileName static methods."
    ),
    Rule(
        rule_id="WX_DEP_AUI_PANE_BUTTON",
        category="deprecated_api",
        severity="ERROR",
        pattern=re.compile(r'\bwxAuiPaneButton\b'),
        description="wxAuiPaneButton was removed in wx 3.2.",
        suggestion="Use plain integer button identifiers."
    ),
    Rule(
        rule_id="WX_DEP_SIZER_RECALC",
        category="deprecated_api",
        severity="WARNING",
        pattern=re.compile(r'->RecalcSizes\s*\('),
        description="wxSizer::RecalcSizes() shouldn't be called directly in wx 3.2+.",
        suggestion="Call Layout() instead."
    ),
    Rule(
        rule_id="WX_DEP_MACRO_NEWD",
        category="deprecated_api",
        severity="WARNING",
        pattern=re.compile(r'\bnewd\b'),
        description="Legacy 'newd' macro from 2.8 memory tracking is obsolete in wx 3.3.",
        suggestion="Use standard 'new' for wxWidgets child windows, or std::make_unique."
    ),
    Rule(
        rule_id="WX_DEP_MACRO_WXT",
        category="deprecated_api",
        severity="WARNING",
        pattern=re.compile(r'\b(wxT|_T)\s*\('),
        description="wxT() and _T() are obsolete since wx 3.0 Unicode unified build.",
        suggestion="Remove wxT()/_T() and use normal UTF-8 string literals or wxstr()."
    ),
    Rule(
        rule_id="WX_DEP_FROM_DIP_MACRO",
        category="deprecated_api",
        severity="WARNING",
        pattern=re.compile(r'\bFROM_DIP\s*\('),
        description="Legacy FROM_DIP(widget, size) uppercase compatibility macro from main.h.",
        suggestion="Use native widget->FromDIP(size) or FromDIP(size) method."
    ),

    # -------------------------------------------------------------------------
    # 2. SIZER & LAYOUT ANTI-PATTERNS
    # -------------------------------------------------------------------------
    Rule(
        rule_id="WX_SIZER_RAW_FLAGS",
        category="sizer_patterns",
        severity="WARNING",
        pattern=re.compile(r'->Add\s*\([^,]+,\s*\d+\s*,\s*(?:wx[A-Z_]+(?:\s*\|\s*wx[A-Z_]+)*)\s*(?:,\s*[^)]+)?\)'),
        description="Legacy sizer Add() with integer proportion, bitwise flags, and border.",
        suggestion="Modernize to type-safe wxSizerFlags(): e.g. Add(ctrl, wxSizerFlags(proportion).Expand().Border(wxALL, FromDIP(8)))."
    ),
    Rule(
        rule_id="WX_STATICBOX_SIZER_CHILD_HIERARCHY",
        category="sizer_patterns",
        severity="WARNING",
        pattern=re.compile(r'new\s+(?:wx[A-Z]\w+)\s*\(\s*(?:this|parent)\s*,\s*[^,]+,\s*[^,]+.*boxsizer', re.I),
        description="Potential wxStaticBoxSizer parenting issue: child controls should be parented to staticBox->GetStaticBox().",
        suggestion="Pass staticBoxSizer->GetStaticBox() as parent instead of 'this' or dialog parent."
    ),

    # -------------------------------------------------------------------------
    # 3. STRING & FORMATTING ANTI-PATTERNS
    # -------------------------------------------------------------------------
    Rule(
        rule_id="WX_STRING_FORMAT",
        category="modernization",
        severity="WARNING",
        pattern=re.compile(r'\bwxString::Format\s*\('),
        description="Legacy wxString::Format printf-style formatting.",
        suggestion="Use std::format(...) with wxstr(...) per project modern C++ guidelines."
    ),
    Rule(
        rule_id="WX_LEGACY_STRING_STREAM",
        category="modernization",
        severity="WARNING",
        pattern=re.compile(r'wxString\(\)\s*<<'),
        description="Legacy (wxString() << a << b) streaming concatenation.",
        suggestion="Use std::format(\"{}{}\", a, b) and convert via wxstr()."
    ),
    Rule(
        rule_id="WX_LEGACY_I2WS",
        category="modernization",
        severity="WARNING",
        pattern=re.compile(r'\bi2ws\s*\('),
        description="Legacy i2ws(...) int-to-wxString wrapper.",
        suggestion="Use std::to_string or std::format with wxstr(), or pass numeric value directly."
    ),
    Rule(
        rule_id="WX_CSTRDATA_CAST",
        category="modernization",
        severity="WARNING",
        pattern=re.compile(r'\.c_str\(\)\s*[\)\]\,]'),
        description="Direct wxString::c_str() usage when converting to std::string or char pointer.",
        suggestion="Use s.ToStdString() or std::string_view conversion."
    ),

    # -------------------------------------------------------------------------
    # 4. HIGH DPI & BITMAP MODERNIZATION
    # -------------------------------------------------------------------------
    Rule(
        rule_id="WX_RAW_BITMAP_TOOL",
        category="high_dpi",
        severity="OPPORTUNITY",
        pattern=re.compile(r'->AddTool\s*\([^,]+,\s*[^,]+,\s*(?:wxBitmap|wxICON|wxBitmapBundle::FromBitmap)\s*\('),
        description="Toolbar tool created with raw single-resolution wxBitmap instead of dynamic wxBitmapBundle.",
        suggestion="Use wxBitmapBundle::FromSVGFile(...) or multi-resolution bundle for crisp High-DPI rendering."
    ),
    Rule(
        rule_id="WX_HARDCODED_PIXELS_IN_UI",
        category="high_dpi",
        severity="OPPORTUNITY",
        pattern=re.compile(r'(?<!FromDIP\()(?<!dipSize\()(?<!FromDIP\( )(?<!dipSize\( )wxSize(?:\s+\w+)?\s*\(\s*\d{2,}\s*,\s*\d{2,}\s*\)'),
        description="Hardcoded wxSize without FromDIP(...) scaling.",
        suggestion="Wrap dimensions in FromDIP(wxSize(w, h)) for per-monitor High-DPI correctness."
    ),
    Rule(
        rule_id="WX_IMAGELIST_LEGACY",
        category="high_dpi",
        severity="OPPORTUNITY",
        pattern=re.compile(r'\bwxImageList\b'),
        description="wxImageList uses fixed pixel dimensions and doesn't scale across High-DPI monitors in wx 3.2/3.3.",
        suggestion="Migrate to wxBitmapBundle or vector of bundles."
    ),

    # -------------------------------------------------------------------------
    # 5. AWESOME 3.2 / 3.3 MODERN FEATURES OPPORTUNITIES
    # -------------------------------------------------------------------------
    Rule(
        rule_id="WX_TEXT_VALIDATOR_RAW_PTR",
        category="modern_features",
        severity="WARNING",
        pattern=re.compile(r'wxTextValidator\s*\([^,]+,\s*&\w+\)'),
        description="wxTextValidator initialized with raw string pointer for direct mutation.",
        suggestion="Use modern validator with transfer callbacks or validate in OnValidate event."
    ),
    Rule(
        rule_id="WX_OPP_INFOBAR",
        category="modern_features",
        severity="OPPORTUNITY",
        pattern=re.compile(r'wxMessageBox\s*\('),
        description="Modal wxMessageBox used for notifications/hints.",
        suggestion="Consider modern wxInfoBar for non-modal, non-intrusive contextual inline alerts."
    ),
    Rule(
        rule_id="WX_OPP_ACTIVITY_INDICATOR",
        category="modern_features",
        severity="OPPORTUNITY",
        pattern=re.compile(r'\b(wxGauge|SetStatusText\s*\(.*loading)\b', re.I),
        description="Legacy progress gauge or status text for indeterminate loading.",
        suggestion="Consider modern wxActivityIndicator (new in 3.2+) for sleek animated spinners."
    ),
    Rule(
        rule_id="WX_OPP_DARK_MODE_NATIVE",
        category="modern_features",
        severity="OPPORTUNITY",
        pattern=re.compile(r'\b(RegOpenKeyEx|GetSystemMetrics|DwmGetColorizationColor)\b'),
        description="Direct Win32 registry/DWM query for dark mode or theme.",
        suggestion="Use wxSystemAppearance::Get().IsDark() and wxApp::SetAppearance() (modern wx 3.3 native API)."
    ),
    Rule(
        rule_id="WX_OPP_CALL_AFTER",
        category="modern_features",
        severity="OPPORTUNITY",
        pattern=re.compile(r'\b(wxPostEvent|AddPendingEvent)\s*\('),
        description="Legacy manual event posting for deferred execution or thread marshaling.",
        suggestion="Use modern wxEvtHandler::CallAfter([=]() { ... }) with lambdas."
    ),
    Rule(
        rule_id="WX_OPP_SET_LABEL_TEXT",
        category="modern_features",
        severity="OPPORTUNITY",
        pattern=re.compile(r'->SetLabel\s*\('),
        description="SetLabel() interprets '&' as mnemonic accelerator.",
        suggestion="Use SetLabelText() for text that may contain '&' (e.g. file paths, town/player names)."
    ),
]

def scan_file(file_path: Path) -> List[Finding]:
    findings: List[Finding] = []
    try:
        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
    except Exception as e:
        return findings

    for line_idx, line in enumerate(lines, 1):
        stripped = line.strip()
        # Skip pure comment lines
        if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
            continue

        for rule in RULES:
            match = rule.pattern.search(line)
            if match:
                # If rule is WX_HARDCODED_PIXELS_IN_UI and the line already applies High-DPI scaling, skip
                if rule.rule_id == "WX_HARDCODED_PIXELS_IN_UI" and ("FromDIP" in line or "dipSize" in line):
                    continue

                findings.append(Finding(
                    file_path=str(file_path).replace("\\", "/"),
                    line_num=line_idx,
                    line_content=stripped,
                    rule_id=rule.rule_id,
                    category=rule.category,
                    severity=rule.severity,
                    description=rule.description,
                    suggestion=rule.suggestion
                ))
    return findings

def scan_directory(root_dir: Path) -> List[Finding]:
    all_findings: List[Finding] = []
    cpp_extensions = {".cpp", ".h", ".hpp", ".cxx"}
    
    # Exclude vendor directory
    exclude_dirs = {"ext", "build", "build-ninja", ".git", ".gemini", "vcpkg_installed", ".wx_docs"}

    for dirpath, dirnames, filenames in os.walk(root_dir):
        # Filter out directories
        dirnames[:] = [d for d in dirnames if d not in exclude_dirs]
        for filename in filenames:
            path = Path(dirpath) / filename
            if path.suffix.lower() in cpp_extensions:
                all_findings.extend(scan_file(path))
    return all_findings

def print_text_report(findings: List[Finding], summary_only: bool = False):
    print("=" * 80)
    print("  RME Redux - wxWidgets 3.3 Complete Legacy & Modernization Scanner")
    print("=" * 80)

    cat_counts: Dict[str, int] = {}
    sev_counts: Dict[str, int] = {}
    rule_counts: Dict[str, int] = {}

    for f in findings:
        cat_counts[f.category] = cat_counts.get(f.category, 0) + 1
        sev_counts[f.severity] = sev_counts.get(f.severity, 0) + 1
        rule_counts[f.rule_id] = rule_counts.get(f.rule_id, 0) + 1

    print("\nSummary by Severity:")
    for sev in ["ERROR", "WARNING", "OPPORTUNITY"]:
        print(f"  {sev:<12}: {sev_counts.get(sev, 0):>4} occurrences")

    print("\nSummary by Category:")
    for cat, count in sorted(cat_counts.items(), key=lambda x: x[1], reverse=True):
        print(f"  {cat:<25}: {count:>4} occurrences")

    print("\nSummary by Rule ID:")
    for rid, count in sorted(rule_counts.items(), key=lambda x: x[1], reverse=True):
        print(f"  {rid:<35}: {count:>4} occurrences")

    if summary_only:
        return

    print("\n" + "=" * 80)
    print("  Detailed Findings:")
    print("=" * 80)

    current_file = ""
    for f in findings:
        if f.file_path != current_file:
            current_file = f.file_path
            print(f"\n[File: {current_file}]")
        print(f"  Line {f.line_num:>4} [{f.severity}] {f.rule_id}: {f.description}")
        print(f"    Code: {f.line_content[:90]}")
        print(f"    Fix : {f.suggestion}")

def generate_markdown_report(findings: List[Finding]) -> str:
    md = []
    md.append("# wxWidgets 3.3 Complete Modernization Audit Report\n")
    md.append("Automated comprehensive scan of RME Redux codebase for all legacy wxWidgets patterns, deprecated APIs, and 3.2/3.3 modern upgrade opportunities.\n")

    sev_counts: Dict[str, int] = {}
    cat_counts: Dict[str, int] = {}
    rule_counts: Dict[str, int] = {}
    file_counts: Dict[str, int] = {}

    for f in findings:
        sev_counts[f.severity] = sev_counts.get(f.severity, 0) + 1
        cat_counts[f.category] = cat_counts.get(f.category, 0) + 1
        rule_counts[f.rule_id] = rule_counts.get(f.rule_id, 0) + 1
        file_counts[f.file_path] = file_counts.get(f.file_path, 0) + 1

    md.append("## Executive Summary\n")
    md.append("| Metric | Count |")
    md.append("| :--- | :--- |")
    md.append(f"| Total Findings | **{len(findings)}** |")
    md.append(f"| Errors (Deprecated/Broken) | **{sev_counts.get('ERROR', 0)}** |")
    md.append(f"| Warnings (Legacy Anti-Patterns) | **{sev_counts.get('WARNING', 0)}** |")
    md.append(f"| Modernization Opportunities | **{sev_counts.get('OPPORTUNITY', 0)}** |")
    md.append(f"| Affected Files | **{len(file_counts)}** |\n")

    md.append("### Findings by Category\n")
    md.append("| Category | Rule ID | Occurrences | Description |")
    md.append("| :--- | :--- | :--- | :--- |")
    for rule in RULES:
        count = rule_counts.get(rule.rule_id, 0)
        if count > 0:
            md.append(f"| `{rule.category}` | `{rule.rule_id}` | {count} | {rule.description} |")
    md.append("\n")

    md.append("### All Affected Files Breakdown\n")
    md.append("| File | Findings Count |")
    md.append("| :--- | :--- |")
    for fp, count in sorted(file_counts.items(), key=lambda x: x[1], reverse=True):
        md.append(f"| [{Path(fp).name}]({fp}) | {count} |")
    md.append("\n")

    return "\n".join(md)

def main():
    parser = argparse.ArgumentParser(description="Scan codebase for all legacy wxWidgets patterns.")
    parser.add_argument("--dir", default="source", help="Directory to scan (default: source)")
    parser.add_argument("--format", choices=["text", "md", "json"], default="text", help="Output format")
    parser.add_argument("--category", help="Filter by specific category")
    parser.add_argument("--summary", action="store_true", help="Print summary only")
    parser.add_argument("--output", help="Write report to file")
    args = parser.parse_args()

    root_dir = Path(args.dir)
    if not root_dir.exists():
        print(f"Error: directory '{args.dir}' does not exist.", file=sys.stderr)
        sys.exit(1)

    findings = scan_directory(root_dir)

    if args.category:
        findings = [f for f in findings if f.category.lower() == args.category.lower()]

    if args.format == "json":
        data = [f._asdict() for f in findings]
        out_str = json.dumps(data, indent=2)
    elif args.format == "md":
        out_str = generate_markdown_report(findings)
    else:
        if args.output:
            import io
            old_stdout = sys.stdout
            buf = io.StringIO()
            sys.stdout = buf
            print_text_report(findings, summary_only=args.summary)
            sys.stdout = old_stdout
            out_str = buf.getvalue()
        else:
            print_text_report(findings, summary_only=args.summary)
            out_str = ""

    if args.output and out_str:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(out_str)
        print(f"Report written to {args.output}")

if __name__ == "__main__":
    main()
