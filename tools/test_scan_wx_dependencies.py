#!/usr/bin/env python3
"""
Unit tests for tools/scan_wx_dependencies.py
"""

import unittest
import tempfile
import json
from pathlib import Path

# Add tools to sys.path
import sys
TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import scan_wx_dependencies as scanner

class TestScanWxDependencies(unittest.TestCase):

    def test_wx_include_pattern(self):
        self.assertTrue(scanner.RE_WX_INCLUDE.search('#include <wx/string.h>'))
        self.assertTrue(scanner.RE_WX_INCLUDE.search('#include "wx/window.h"'))
        self.assertTrue(scanner.RE_WX_INCLUDE.search('  #include <wx/dir.h>'))
        self.assertFalse(scanner.RE_WX_INCLUDE.search('#include <string>'))
        self.assertFalse(scanner.RE_WX_INCLUDE.search('#include "game/item.h"'))

    def test_wx_type_pattern(self):
        self.assertTrue(scanner.RE_WX_TYPE.search('wxString str = "hello";'))
        self.assertTrue(scanner.RE_WX_TYPE.search('const wxPoint& pt'))
        self.assertTrue(scanner.RE_WX_TYPE.search('std::unique_ptr<wxWindow> win;'))
        self.assertTrue(scanner.RE_WX_TYPE.search('wxDateTime dt;'))
        self.assertFalse(scanner.RE_WX_TYPE.search('std::string str;'))
        self.assertFalse(scanner.RE_WX_TYPE.search('Position pos;'))

    def test_wx_macro_pattern(self):
        self.assertTrue(scanner.RE_WX_MACRO.search('wxstr("text")'))
        self.assertTrue(scanner.RE_WX_MACRO.search('wxT("literal")'))
        self.assertTrue(scanner.RE_WX_MACRO.search('int id = wxID_OK;'))
        self.assertTrue(scanner.RE_WX_MACRO.search('return wxEmptyString;'))
        self.assertFalse(scanner.RE_WX_MACRO.search('std::format("text")'))

    def test_gui_leak_pattern(self):
        self.assertTrue(scanner.RE_GUI_LEAK.search('g_gui.GetSpawnTime()'))
        self.assertTrue(scanner.RE_GUI_LEAK.search('#include "ui/gui.h"'))
        self.assertTrue(scanner.RE_GUI_LEAK.search('DialogUtil::PopupDialog("Error", msg);'))
        self.assertFalse(scanner.RE_GUI_LEAK.search('editor.map.getTile(pos)'))
        self.assertFalse(scanner.RE_GUI_LEAK.search('#include "game/spawn.h"'))

    def test_cmake_parser(self):
        cmake_path = REPO_ROOT / 'source' / 'CMakeLists.txt'
        core_files = scanner.parse_cmake_core_files(cmake_path)
        self.assertGreater(len(core_files), 300)
        self.assertIn('source/game/item.cpp', core_files)
        self.assertIn('source/map/map.cpp', core_files)
        self.assertIn('source/brushes/brush.cpp', core_files)
        # Verify UI files are NOT in rme_core
        self.assertNotIn('source/ui/main_frame.cpp', core_files)
        self.assertNotIn('source/palette/palette_window.cpp', core_files)

    def test_scan_synthetic_file(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmppath = Path(tmpdir)
            source_dir = tmppath / 'source' / 'brushes'
            source_dir.mkdir(parents=True)
            dirty_file = source_dir / 'test_brush.cpp'
            dirty_file.write_text(
                '#include "app/main.h"\n'
                '#include <wx/string.h>\n'
                'wxString s = wxstr("bad");\n'
                'g_gui.RefreshView();\n',
                encoding='utf-8'
            )

            violations = scanner.scan_file(dirty_file, tmppath)
            rule_types = {v.rule_type for v in violations}
            self.assertIn('MAIN_H_LEAK', rule_types)
            self.assertIn('WX_INCLUDE', rule_types)
            self.assertIn('WX_TYPE', rule_types)
            self.assertIn('WX_MACRO', rule_types)
            self.assertIn('GUI_LEAK', rule_types)

    def test_rme_core_is_completely_pure(self):
        """Strict verification: rme_core must have ZERO wx/GUI violations."""
        violations, stats, total_files = scanner.run_scan(REPO_ROOT, 'rme_core')
        self.assertEqual(len(violations), 0, f"Violations found in rme_core: {violations}")
        self.assertGreater(total_files, 350)

    def test_json_formatter(self):
        violations, stats, total_files = scanner.run_scan(REPO_ROOT, 'rme_core')
        json_str = scanner.format_json(violations, stats, total_files, 'rme_core')
        data = json.loads(json_str)
        self.assertEqual(data['target_mode'], 'rme_core')
        self.assertEqual(data['total_violations'], 0)

if __name__ == '__main__':
    unittest.main()
