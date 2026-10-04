import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from patch_prisma import patch_sources


def main():
    test_folder = Path(__file__).resolve().parent
    vendor = test_folder.parents[1] / "ThirdParty/libprisma/libprisma"
    original = tuple((vendor / name).read_text(encoding="utf-8")
                     for name in ("SyntaxHighlighter.h", "SyntaxHighlighter.cpp"))
    patched = patch_sources(*original)
    assert patch_sources(*patched) == patched
    assert "&nestedRematch, depth + 1" in patched[1]
    compiler = shutil.which("cl")
    if not compiler:
        raise RuntimeError("Run from an MSVC developer shell.")
    with tempfile.TemporaryDirectory(prefix="ayu-prisma-test-") as temp:
        folder = Path(temp)
        for name in ("TokenList.h", "TokenList.cpp"):
            shutil.copyfile(vendor / name, folder / name)
        body = patched[1][patched[1].index("TokenList SyntaxHighlighter::tokenize(std::string_view"):]
        (folder / "grammar_under_test.h").write_text(body, encoding="utf-8")
        subprocess.run([compiler, "/nologo", "/std:c++20", "/EHsc", "/W4", "/Od",
                        f"/I{folder}", str(test_folder / "prisma_depth_test.cpp"),
                        str(folder / "TokenList.cpp"), "/Fe:test.exe"], cwd=folder, check=True)
        subprocess.run([str(folder / "test.exe")], cwd=folder, check=True)


if __name__ == "__main__":
    main()
