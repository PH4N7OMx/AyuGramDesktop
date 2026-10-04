#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include "TokenList.h"
struct Grammar;
struct Pattern {
    const Grammar *nested = nullptr;
    const Grammar *inside() const { return nested; }
    bool greedy() const { return false; }
    std::string alias() const { return {}; }
    std::string_view match(bool &success, size_t &pos, std::string_view text) const {
        success = !text.empty();
        pos = 0;
        return text;
    }
};
struct GrammarToken : std::vector<std::shared_ptr<Pattern>> {
    const std::string &name() const { static const std::string value = "test"; return value; }
};
struct Grammar { std::vector<GrammarToken> tokens; };
struct RematchOptions { std::string token; size_t reach; int j; };
class SyntaxHighlighter {
public:
    TokenList tokenize(std::string_view text, const Grammar *grammar, size_t depth = 0);
    void matchGrammar(std::string_view text, TokenList &tokens, const Grammar *grammar, TokenListPtr start, size_t pos, RematchOptions *rematch, size_t depth);
};
#include "grammar_under_test.h"
std::string flatten(const TokenList &tokens, int depth = 0) {
    assert(depth <= 32);
    std::string result;
    for (const auto &node : tokens) {
        if (node.isSyntax()) {
            result += flatten(dynamic_cast<const Syntax &>(node).children(), depth + 1);
        } else {
            result += dynamic_cast<const Text &>(node).value();
        }
    }
    return result;
}
int main() {
    Grammar cycle;
    auto pattern = std::make_shared<Pattern>();
    pattern->nested = &cycle;
    GrammarToken token;
    token.push_back(pattern);
    cycle.tokens.push_back(token);
    SyntaxHighlighter highlighter;
    const auto text = std::string("a code fragment");
    for (auto i = 0; i != 100; ++i) {
        const auto result = highlighter.tokenize(text, &cycle);
        assert(flatten(result) == text);
    }
    pattern->nested = nullptr;
    const auto ordinary = highlighter.tokenize(text, &cycle);
    assert(flatten(ordinary) == text);
    const auto plain = highlighter.tokenize(text, &cycle, 32);
    assert(!plain.begin()->isSyntax());
    assert(flatten(plain) == text);
    std::cout << "Prisma checks passed: cyclic grammar, depth cutoff, text preservation and repeated destruction.\n";
}
