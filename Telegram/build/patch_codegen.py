import re
from pathlib import Path


def patch_source(source):
    pattern = (
        r"if \(data\[i\] != 'l'\s*"
        r"\|\| data\[i \+ 1\] != 'n'\s*"
        r"\|\| data\[i \+ 2\] != 'g'\s*"
        r"\|\| data\[i \+ 3\] != '_'"
    )
    replacement = (
        "const auto isLng = (data[i] == 'l' && data[i + 1] == 'n'"
        " && data[i + 2] == 'g' && data[i + 3] == '_');\n\t\t"
        "const auto isAyu = (data[i] == 'a' && data[i + 1] == 'y'"
        " && data[i + 2] == 'u' && data[i + 3] == '_');\n\t\t"
        "if ((!isLng && !isAyu)"
    )
    patched, count = re.subn(pattern, lambda _: replacement, source)
    if count != 1 and not (count == 0 and replacement in source):
        raise RuntimeError("Unexpected codegen scanner; ayu_ patch was not applied.")
    patched, versions = re.subn(
        r"constexpr auto kCacheVersion = quint32\([12]\);",
        "constexpr auto kCacheVersion = quint32(2);", patched,
    )
    if versions != 1:
        raise RuntimeError("Unexpected codegen cache version.")
    return patched


def main():
    path = Path(__file__).resolve().parents[1] / "codegen/codegen/lang/subsets.cpp"
    source = path.read_text(encoding="utf-8")
    patched = patch_source(source)
    if patched != source:
        path.write_bytes(patched.replace("\n", "\r\n").encode("utf-8"))
    print("Codegen scanner supports lng_ and ayu_; cache version 2.")


if __name__ == "__main__":
    main()
