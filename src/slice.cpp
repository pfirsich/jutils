#include <functional>
#include <iostream>

#include <clipp/clipp.hpp>

#include "io.hpp"
#include "util.hpp"

namespace {
struct SliceArgs : clipp::ArgsBase {
    std::optional<int64_t> offset;
    std::optional<int64_t> num;
    std::optional<int64_t> step = 1;

    void args()
    {
        flag(offset, "offset", 'o');
        flag(num, "num", 'n');
        flag(step, "step", 's');
    }

    std::string description() const override
    {
        return R"(
jslice -n 15 # first 15 rows
jslice -o -15 # last 15 rows
jslice -o 5 -n 10 # 10 elements, starting at index 5 (the 6th element)
jslice -o 5 -n 10 -s 2 # same as above, but take every other (2nd) element
jslice -o 5 -n 10 -s -2 # same as above, but reversed
jslice -s -1 # all rows, but reversed
)";
    }
};
}

int slice(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<SliceArgs>(argc, argv).value();

    auto step = args.step.value();
    if (step == 0) {
        std::cerr << "step must be != 0" << std::endl;
        return 1;
    }

    Input input;

    // For now this will sponge every time, because it's easier to do
    std::vector<std::vector<Value>> rows = input.rows();

    auto offset = [&]() {
        if (args.offset) {
            return *args.offset >= 0 ? *args.offset : rows.size() + *args.offset;
        } else {
            return step > 0 ? 0 : rows.size() - 1;
        }
    }();

    auto num = args.num;
    if (num && *num < 0) {
        num = rows.size() + *num;
    }

    Output output(input.columns());

    int64_t numOutput = 0;
    for (int64_t i = offset; i >= 0 && i < static_cast<int64_t>(rows.size()); i += step) {
        output.row(rows[i]);
        numOutput++;
        if (num && numOutput >= *num) {
            break;
        }
    }

    return 0;
}
