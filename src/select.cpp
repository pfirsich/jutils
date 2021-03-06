#include <iostream>

#include <clipp/clipp.hpp>

#include "io.hpp"
#include "util.hpp"

namespace {
struct SelectArgs : clipp::ArgsBase {
    std::vector<std::string> columns;

    void args() { positional(columns, "columns"); }
};
}

int select(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<SelectArgs>(argc, argv).value();

    Input input;

    std::vector<size_t> columnIndices;
    for (const auto& col : args.columns) {
        const auto idx = getColumnIndex(input.columns(), col);
        if (!idx) {
            std::cerr << "Invalid column '" << col << "'" << std::endl;
            return 1;
        }
        columnIndices.push_back(idx.value());
    }

    std::vector<Column> columns;
    for (const auto& idx : columnIndices) {
        columns.push_back(input.columns()[idx]);
    }

    Output output(columns);

    while (const auto row = input.row()) {
        std::vector<Value> values;
        for (const auto& idx : columnIndices) {
            values.push_back(row.value()[idx]);
        }
        output.row(values);
    }

    return 0;
}
