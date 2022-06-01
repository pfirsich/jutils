#include <iostream>

#include "../../cli/clipp.hpp"

#include "io.hpp"
#include "util.hpp"

namespace {
struct PrintArgs : clipp::ArgsBase {
    std::optional<int64_t> colWidth;
    bool sponge = false;

    void args()
    {
        flag(sponge, "sponge", 's').help("Sponge");
        flag(colWidth, "colwidth", 'c').help("Column width");
    }
};

void printColumn(const std::string str, size_t colWidth)
{
    const auto padding = colWidth > str.size() ? colWidth - str.size() : 1;
    std::cout << str << std::string(padding, ' ');
}
}

int print(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<PrintArgs>(argc, argv).value();

    Input input;
    if (args.sponge) {
        const auto& columns = input.columns();
        std::vector<size_t> colWidths;
        colWidths.reserve(columns.size());
        for (const auto& col : input.columns()) {
            colWidths.push_back(col.name.size() + 1);
        }

        std::vector<std::vector<std::string>> rows;
        while (const auto row = input.row()) {
            rows.push_back({});
            for (size_t i = 0; i < row->size(); ++i) {
                const auto str = toString(row.value()[i]);
                rows.back().push_back(str);
                colWidths[i] = std::max(colWidths[i], str.size() + 1);
            }
        }

        if (args.colWidth) {
            colWidths.assign(colWidths.size(), *args.colWidth);
        }

        size_t fullWidth = 0;
        for (const auto w : colWidths) {
            fullWidth += w;
        }

        for (size_t i = 0; i < columns.size(); ++i) {
            printColumn(columns[i].name, colWidths[i]);
        }

        std::cout << "\n" << std::string(fullWidth, '-') << std::endl;

        for (const auto& row : rows) {
            for (size_t i = 0; i < columns.size(); ++i) {
                printColumn(row[i], colWidths[i]);
            }
            std::cout << std::endl;
        }
    } else {
        const size_t colWidth = args.colWidth.value_or(16);

        for (const auto& col : input.columns()) {
            printColumn(col.name, colWidth);
        }

        std::cout << "\n" << std::string(colWidth * input.columns().size(), '-') << std::endl;

        while (const auto row = input.row()) {
            for (const auto& val : row.value()) {
                printColumn(toString(val), colWidth);
            }
            std::cout << std::endl;
        }
    }

    return 0;
}
