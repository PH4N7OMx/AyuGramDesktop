from pathlib import Path


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError("Unexpected libprisma source layout.")
    return source.replace(old, new, 1)


def patch_sources(header, source):
    if "size_t depth = 0" in header:
        if "depth >= 32" not in source:
            raise RuntimeError("Incomplete libprisma recursion patch.")
        return header, source
    header = replace_once(header,
        "TokenList tokenize(std::string_view text, const Grammar* grammar);",
        "TokenList tokenize(std::string_view text, const Grammar* grammar, size_t depth = 0);")
    header = replace_once(header,
        "size_t startPos, RematchOptions* rematch);",
        "size_t startPos, RematchOptions* rematch, size_t depth);")
    source = replace_once(source,
        "TokenList SyntaxHighlighter::tokenize(std::string_view text, const Grammar* grammar)",
        "TokenList SyntaxHighlighter::tokenize(std::string_view text, const Grammar* grammar, size_t depth)")
    source = replace_once(source,
        "matchGrammar(text, tokenList, grammar, tokenList.head, 0, nullptr);",
        "matchGrammar(text, tokenList, grammar, tokenList.head, 0, nullptr, depth);")
    source = replace_once(source,
        "size_t startPos, RematchOptions* rematch)\n{",
        "size_t startPos, RematchOptions* rematch, size_t depth)\n{\n"
        "    if (depth >= 32)\n    {\n        return;\n    }\n")
    source = replace_once(source,
        "return tokenize(match, inside);",
        "return tokenize(match, inside, depth + 1);")
    source = replace_once(source,
        "matchGrammar(text, tokenList, grammar, currentNode->prev, pos, &nestedRematch);",
        "matchGrammar(text, tokenList, grammar, currentNode->prev, pos, &nestedRematch, depth + 1);")
    return header, source


def main():
    folder = Path(__file__).resolve().parents[1] / "ThirdParty/libprisma/libprisma"
    header_path = folder / "SyntaxHighlighter.h"
    source_path = folder / "SyntaxHighlighter.cpp"
    original = (header_path.read_text(encoding="utf-8"), source_path.read_text(encoding="utf-8"))
    patched = patch_sources(*original)
    for path, before, after in zip((header_path, source_path), original, patched):
        if before != after:
            path.write_text(after, encoding="utf-8", newline="\n")
    print("libprisma recursion depth limited to 32.")


if __name__ == "__main__":
    main()
