#include <algorithm>
#include <iostream>
#include <regex>
#include <unordered_set>

#include "../../cli/clipp.hpp"

#include "io.hpp"
#include "util.hpp"

namespace {
struct FilterArgs : clipp::ArgsBase {
    std::optional<std::string> unique; // TODO: Later allow expressions for this?

    void args() { flag(unique, "unique", 'u'); }
};

struct Expr {
    virtual bool eval(const std::vector<Value>& row) const = 0;
    virtual ~Expr() = default;
};

struct TrueExpr : public Expr {
    bool eval(const std::vector<Value>&) const override { return true; }
};

struct ContainsExpr : public Expr {
    size_t column;
    std::string needle;

    ContainsExpr(size_t column, std::string needle)
        : column(column)
        , needle(needle)
    {
    }

    bool eval(const std::vector<Value>& row) const override
    {
        return std::get<std::string>(row[column]).find(needle) != std::string::npos;
    }
};

struct EqExpr : public Expr {
    size_t column;
    std::string rhs;

    EqExpr(size_t column, std::string rhs)
        : column(column)
        , rhs(rhs)
    {
    }

    bool eval(const std::vector<Value>& row) const override
    {
        return std::get<std::string>(row[column]) == rhs;
    }
};

struct RegexMatchExpr : public Expr {
    size_t column;
    std::string pattern;
    std::regex regex;

    RegexMatchExpr(size_t column, std::string pattern)
        : column(column)
        , pattern(pattern)
        , regex(pattern)
    {
    }

    bool eval(const std::vector<Value>& row) const override
    {
        std::smatch m;
        return std::regex_search(std::get<std::string>(row[column]), m, regex);
    }
};

std::unique_ptr<Expr> parseExpr(
    const std::vector<std::string>& where, const std::vector<Column>& columns)
{
    assert(where.size() == 3);
    const auto& lhs = where[0];
    const auto& op = where[1];
    const auto& rhs = where[2];

    const auto idx = getColumnIndex(columns, lhs);
    if (!idx) {
        std::cerr << "Invalid column name: " << lhs << std::endl;
        std::exit(3);
    }

    if (op == "contains") {
        assert(columns[*idx].type == Column::Type::String);
        return std::make_unique<ContainsExpr>(*idx, rhs);
    } else if (op == "==") {
        assert(columns[*idx].type == Column::Type::String);
        return std::make_unique<EqExpr>(*idx, rhs);
    } else if (op == "=~") {
        assert(columns[*idx].type == Column::Type::String);
        return std::make_unique<RegexMatchExpr>(*idx, rhs);
    } else {
        std::cerr << "Invalid operation: " << op << std::endl;
        std::exit(4);
    }
}
}

int filter(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    parser.errorOnExtraArgs(false);
    const auto args = parser.parse<FilterArgs>(argc, argv).value();

    Input input;

    const auto& exprTokens = args.remaining();
    auto expr = !exprTokens.empty() ? parseExpr(exprTokens, input.columns())
                                    : std::make_unique<TrueExpr>();

    Output output(input.columns());

    if (args.unique) {
        const auto idxOpt = getColumnIndex(input.columns(), *args.unique);
        if (!idxOpt) {
            std::cerr << "Invalid column: " << *args.unique << std::endl;
            return 1;
        }
        const auto idx = idxOpt.value();
        // TODO: Somehow build the uniqueness check into expr
        std::unordered_set<Value> seen;
        while (const auto row = input.row()) {
            const auto res = seen.insert(row.value()[idx]);
            if (res.second && expr->eval(*row)) {
                output.row(row.value());
            }
        }
    } else {
        while (const auto row = input.row()) {
            if (expr->eval(*row)) {
                output.row(row.value());
            }
        }
    }

    return 0;
}
