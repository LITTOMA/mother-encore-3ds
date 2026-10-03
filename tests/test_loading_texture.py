"""Exercise the actual progressive loader with public GPU API test doubles."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LoadingTextureTests(unittest.TestCase):
    def test_progress_payload_ownership_and_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "loading-texture-tests"
            subprocess.run([
                os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fno-exceptions", "-fno-rtti", "-I", str(ROOT / "tests/loading_texture_stubs"),
                "-I", str(ROOT), "-I", str(ROOT / "include"),
                str(ROOT / "tests/loading_texture_tests.cpp"), "-o", str(output),
            ], check=True)
            subprocess.run([str(output), directory], check=True)


if __name__ == "__main__":
    unittest.main()
