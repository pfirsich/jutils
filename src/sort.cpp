#include <algorithm>
#include <iostream>

#include "../../cli/clipp.hpp"

#include "io.hpp"

struct SortArgs : clipp::ArgsBase {
    bool reverse = false;
    std::string column;

    void args()
    {
        flag(reverse, "reverse", 'r');
        positional(column, "column");
    }
};

std::optional<size_t> getColumnIndex(const std::vector<Column>& columns, const std::string& column)
{
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].name == column) {
            return i;
        }
    }
    return std::nullopt;
}

bool compare(const Value& a, const Value& b)
{
    assert(a.index() == b.index());
    if (a.index() == 0) {
        return std::get<0>(a) < std::get<0>(b);
    } else if (a.index() == 1) {
        return std::get<1>(a) < std::get<1>(b);
    } else {
        std::abort();
    }
}

int sort(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<SortArgs>(argc, argv).value();

    Input input;

    const auto idx = getColumnIndex(input.columns(), args.column);
    if (!idx) {
        std::cerr << "Invalid column" << std::endl;
        return 1;
    }

    Output output(input.columns());

    std::vector<std::vector<Value>> rows;
    while (const auto row = input.row()) {
        rows.push_back(row.value());
    }

    std::stable_sort(rows.begin(), rows.end(),
        [idx = idx.value(), reverse = args.reverse](const auto& a, const auto& b) {
            const auto less = compare(a[idx], b[idx]);
            return reverse ? !less : less;
        });

    for (const auto& row : rows) {
        output.row(row);
    }

    return 0;
}
