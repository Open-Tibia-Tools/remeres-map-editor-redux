#!/usr/bin/env python3
"""
Unit tests for tools/scan_legacy_wx.py
"""

import unittest
from pathlib import Path
import tempfile
import os

from scan_legacy_wx import scan_file, RULES

class TestScanLegacyWx(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.dir_path = Path(self.temp_dir.name)

    def tearDown(self):
        self.temp_dir.cleanup()

    def _create_and_scan(self, content: str):
        test_file = self.dir_path / "test_snippet.cpp"
        with open(test_file, "w", encoding="utf-8") as f:
            f.write(content)
        return scan_file(test_file)

    def test_detect_event_table(self):
        code = """
        BEGIN_EVENT_TABLE(MyDialog, wxDialog)
            EVT_BUTTON(wxID_OK, MyDialog::OnOk)
        END_EVENT_TABLE()
        """
        findings = self._create_and_scan(code)
        rule_ids = [f.rule_id for f in findings]
        self.assertIn("WX_DEP_EVENT_TABLE", rule_ids)

    def test_detect_wxstring_format(self):
        code = """
        wxString s = wxString::Format("Count: %d", 42);
        """
        findings = self._create_and_scan(code)
        rule_ids = [f.rule_id for f in findings]
        self.assertIn("WX_STRING_FORMAT", rule_ids)

    def test_detect_raw_sizer_flags(self):
        code = """
        mainSizer->Add(btn, 0, wxALL | wxEXPAND, 5);
        """
        findings = self._create_and_scan(code)
        rule_ids = [f.rule_id for f in findings]
        self.assertIn("WX_SIZER_RAW_FLAGS", rule_ids)

    def test_detect_newd_macro(self):
        code = """
        auto* btn = newd wxButton(this, wxID_OK);
        """
        findings = self._create_and_scan(code)
        rule_ids = [f.rule_id for f in findings]
        self.assertIn("WX_DEP_MACRO_NEWD", rule_ids)

    def test_detect_hardcoded_pixels(self):
        code = """
        wxSize sz(300, 200);
        """
        findings = self._create_and_scan(code)
        rule_ids = [f.rule_id for f in findings]
        self.assertIn("WX_HARDCODED_PIXELS_IN_UI", rule_ids)

    def test_detect_makemodal(self):
        code = """
        dialog->MakeModal(true);
        """
        findings = self._create_and_scan(code)
        rule_ids = [f.rule_id for f in findings]
        self.assertIn("WX_DEP_MAKE_MODAL", rule_ids)

    def test_detect_imagelist_legacy(self):
        code = """
        wxImageList* imgList = new wxImageList(16, 16);
        """
        findings = self._create_and_scan(code)
        rule_ids = [f.rule_id for f in findings]
        self.assertIn("WX_IMAGELIST_LEGACY", rule_ids)

    def test_skip_comments(self):
        code = """
        // BEGIN_EVENT_TABLE(MyDialog, wxDialog)
        /* wxString::Format("Not real code"); */
        """
        findings = self._create_and_scan(code)
        self.assertEqual(len(findings), 0)

if __name__ == "__main__":
    unittest.main()
