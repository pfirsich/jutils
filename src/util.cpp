#include "util.hpp"

#include <iostream>

#include <fcntl.h>
#include <unistd.h>

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

std::optional<std::string> readFile(const std::string& path)
{
    auto fd = ::open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        return std::nullopt;
    }
    char readBuffer[4096];
    std::string ret;
    int res = 0;
    while ((res = ::read(fd, readBuffer, sizeof(readBuffer))) > 0) {
        ret.append(std::string_view(readBuffer, res));
    }
    ::close(fd);
    if (res < 0) {
        return std::nullopt;
    }
    return ret;
}
