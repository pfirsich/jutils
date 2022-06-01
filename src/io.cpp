#include "io.hpp"

#include <cassert>
#include <cstring>

#include <unistd.h>

constexpr size_t MagicLen = 4;
constexpr char Magic[MagicLen + 1] = "\xe9SIO";

constexpr char RowStart[MagicLen + 1] = "\xe9ROW";

using StringLen = uint16_t;
using ColumnCount = uint32_t;
using ColumnType = uint8_t;

Output::Output(std::vector<Column> columns)
    : columns_(std::move(columns))
    , stdoutIsATty_(::isatty(STDOUT_FILENO))
{
    ::write(STDOUT_FILENO, Magic, MagicLen);
    const ColumnCount columnCount = columns_.size(); // TODO: byte order
    ::write(STDOUT_FILENO, &columnCount, sizeof(columnCount));
    for (const auto& col : columns_) {
        const auto type = static_cast<ColumnType>(col.type);
        ::write(STDOUT_FILENO, &type, sizeof(type));
        const auto nameLen = static_cast<StringLen>(col.name.size());
        ::write(STDOUT_FILENO, &nameLen, sizeof(nameLen));
        ::write(STDOUT_FILENO, col.name.data(), col.name.size());
    }
}

void Output::row(const std::vector<Value>& values) const
{
    ::write(STDOUT_FILENO, RowStart, MagicLen);
    for (size_t i = 0; i < values.size(); ++i) {
        if (const auto valI64 = std::get_if<int64_t>(&values[i])) {
            assert(columns_[i].type == Column::Type::I64);
            ::write(STDOUT_FILENO, valI64, sizeof(std::remove_pointer_t<decltype(valI64)>));
        } else if (const auto valStr = std::get_if<std::string>(&values[i])) {
            assert(columns_[i].type == Column::Type::String);
            const auto len = static_cast<StringLen>(valStr->size());
            ::write(STDOUT_FILENO, &len, sizeof(len));
            ::write(STDOUT_FILENO, valStr->data(), valStr->size());
        }
    }
}

Input::Input()
    : stdinIsATty_(::isatty(STDIN_FILENO))
{
    char magic[MagicLen];
    auto res = ::read(STDIN_FILENO, magic, MagicLen); // TODO
    assert(res > 0);
    assert(std::memcmp(Magic, magic, MagicLen) == 0);
    ColumnCount columnCount = 0;
    res = ::read(STDIN_FILENO, &columnCount, sizeof(columnCount));
    assert(res > 0);
    for (size_t i = 0; i < columnCount; ++i) {
        ColumnType type = 0;
        res = ::read(STDIN_FILENO, &type, sizeof(type));
        assert(res > 0);
        StringLen len = 0;
        res = ::read(STDIN_FILENO, &len, sizeof(len));
        assert(res > 0);
        std::string name(len, 0);
        res = ::read(STDIN_FILENO, name.data(), len);
        assert(res > 0);
        columns_.push_back(Column { std::move(name), static_cast<Column::Type>(type) });
    }
}

std::optional<std::vector<Value>> Input::row() const
{
    char rowStart[MagicLen];
    auto res = ::read(STDIN_FILENO, rowStart, MagicLen);
    if (res == 0) {
        return std::nullopt;
    }
    assert(res > 0);
    assert(std::memcmp(RowStart, rowStart, MagicLen) == 0);
    std::vector<Value> values;
    for (size_t i = 0; i < columns_.size(); ++i) {
        switch (columns_[i].type) {
        case Column::Type::I64: {
            int64_t value = 0;
            res = ::read(STDIN_FILENO, &value, sizeof(value));
            assert(res > 0);
            values.push_back(value);
            break;
        }
        case Column::Type::String: {
            StringLen len = 0;
            res = ::read(STDIN_FILENO, &len, sizeof(len));
            assert(res > 0);
            std::string str(len, 0);
            res = ::read(STDIN_FILENO, str.data(), len);
            assert(res > 0);
            values.push_back(str);
            break;
        }
        case Column::Type::Invalid:
            std::abort();
        }
    }
    return values;
}
