import os
import shutil
import subprocess
import tempfile
from pathlib import Path


def main():
    tests = Path(__file__).resolve().parent
    source = (tests.parent / "filters_controller.cpp").read_text(encoding="utf-8")
    start = source.index("namespace {\n\nconstexpr auto kDuplicateFastLookupLimit")
    end = source.index("\nvoid handleDuplicateItemRemoved", start)
    compiler = os.environ.get("CXX") or next(
        (path for name in ("cl", "clang++", "g++")
         if (path := shutil.which(name))), None,
    )
    if not compiler:
        raise RuntimeError("Run in a compiler environment or set CXX.")
    with tempfile.TemporaryDirectory(prefix="ayu-duplicate-test-") as directory:
        output = Path(directory)
        (output / "duplicate_group_under_test.h").write_text(
            source[start:end], encoding="utf-8",
        )
        executable = output / ("test.exe" if os.name == "nt" else "test")
        test = tests / "duplicate_group_test.cpp"
        if Path(compiler).stem.lower() == "cl":
            flags = ["/nologo", "/std:c++20", "/EHsc", "/W4", "/Od",
                     f"/I{output}", f"/Fe{executable}", f"/Fo{output / 'test.obj'}"]
        else:
            flags = ["-std=c++20", "-Wall", "-Wextra", "-O0",
                     "-I", str(output), "-o", str(executable)]
        subprocess.run([compiler, *flags, str(test)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
