#include <algorithm>
#include <iostream>

#include "../../cli/clipp.hpp"

#include "io.hpp"
#include "util.hpp"

namespace {
struct FilterArgs : clipp::ArgsBase {
    std::optional<std::string> unique;

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

    assert(!args.unique);

    const auto& exprTokens = args.remaining();
    auto expr = !exprTokens.empty() ? parseExpr(exprTokens, input.columns())
                                    : std::make_unique<TrueExpr>();

    Output output(input.columns());

    std::vector<std::vector<Value>> rows;
    while (const auto row = input.row()) {
        if (expr->eval(*row)) {
            output.row(row.value());
        }
    }

    return 0;
}
