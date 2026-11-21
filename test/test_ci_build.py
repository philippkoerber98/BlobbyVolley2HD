import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("ci_build", Path(__file__).resolve().parents[1] / "tools/ci_build.py")
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)


class BuildVersionTest(unittest.TestCase):
    def test_version_and_reruns(self):
        cmake = "project(blobby VERSION 1.2.3)"
        self.assertEqual(ci.build_version(cmake, 17, 1), "1.2.3-build.17.1")
        self.assertNotEqual(ci.build_version(cmake, 17, 1), ci.build_version(cmake, 17, 2))

    def test_invalid_numbers_and_missing_base(self):
        for run, attempt in [(0, 1), (-1, 2), (1, 0), (65536, 1)]:
            with self.assertRaises(ValueError):
                ci.build_version("project(blobby VERSION 1.2.3)", run, attempt)
        with self.assertRaises(ValueError):
            ci.build_version("bad source", 1, 1)


if __name__ == "__main__":
    unittest.main()
