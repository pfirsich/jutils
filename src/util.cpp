#include "util.hpp"

#include <iostream>

std::optional<size_t> getColumnIndex(const std::vector<Column>& columns, const std::string& column)
{
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].name == column) {
            return i;
        }
    }
    return std::nullopt;
}

std::string toString(const Value& val)
{
    if (const auto str = std::get_if<std::string>(&val)) {
        return *str;
    } else if (const auto i64 = std::get_if<int64_t>(&val)) {
        return std::to_string(*i64);
    }
    std::abort();
};
