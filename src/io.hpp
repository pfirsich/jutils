#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

struct Column {
    enum class Type {
        Invalid,
        I64,
        String,
    };

    std::string name;
    Type type;
};
using Value = std::variant<int64_t, std::string>;

class Output {
public:
    Output(std::vector<Column> columns);
    ~Output();

    void row(const std::vector<Value>& values);

private:
    void flush();

    std::vector<Column> columns_;
    std::vector<std::vector<Value>> rows_;
    bool textOutput_;
    bool flushed_ = false;
};

class Input {
public:
    Input();

    std::optional<std::vector<Value>> row() const;
    std::vector<std::vector<Value>> rows() const;

    const auto& columns() const { return columns_; }

private:
    std::vector<Column> columns_;
    bool stdinIsATty_;
};
