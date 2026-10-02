import importlib.util
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "patch_codegen", Path(__file__).resolve().parents[1] / "patch_codegen.py",
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

ORIGINAL = """constexpr auto kCacheVersion = quint32(1);
if (data[i] != 'l'
    || data[i + 1] != 'n'
    || data[i + 2] != 'g'
    || data[i + 3] != '_'
    || (i > 0 && IsIdentifierChar(data[i - 1]))) {
    continue;
}
"""


class PatchCodegenTests(unittest.TestCase):
    def test_original_scanner_and_cache(self):
        patched = module.patch_source(ORIGINAL)
        self.assertIn("const auto isLng", patched)
        self.assertIn("const auto isAyu", patched)
        self.assertIn("kCacheVersion = quint32(2)", patched)
        self.assertIn("IsIdentifierChar(data[i - 1])", patched)
        self.assertEqual(module.patch_source(patched), patched)

    def test_previous_workflow_patch_invalidates_cache(self):
        patched = module.patch_source(ORIGINAL).replace("quint32(2)", "quint32(1)")
        self.assertIn("quint32(2)", module.patch_source(patched))

    def test_unknown_source_fails(self):
        with self.assertRaises(RuntimeError):
            module.patch_source(ORIGINAL.replace("data[i] != 'l'", "data[i] != 'x'"))
        with self.assertRaises(RuntimeError):
            module.patch_source(ORIGINAL.replace("quint32(1)", "quint32(9)"))


if __name__ == "__main__":
    unittest.main()
