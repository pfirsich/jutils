#include <iostream>
#include <regex>

#include "../../cli/clipp.hpp"

#include "io.hpp"
#include "util.hpp"

#include <unistd.h>

namespace {
struct ParseArgs : clipp::ArgsBase {
    std::optional<std::string> rowDelim = "\n";
    std::optional<std::string> regex;
    std::optional<std::string> csv;
    bool trim = false;
    std::vector<std::string> columns;

    void args()
    {
        flag(rowDelim, "rowdelim", 'n').help("The row delimiter.");
        flag(regex, "regex", 'r')
            .help("Match a regular expression columns values are retrieved from match groups.");
        flag(csv, "csv", 'c').help("Split the string at the specified delimeter.");
        flag(trim, "trim", 't').help("Whether to trim strings after matching. Only for --csv");
        positional(columns, "columns");
    }
};

std::vector<std::string_view> split(
    std::string_view str, std::string_view delim, size_t maxParts = 0)
{
    std::vector<std::string_view> parts;
    std::string_view remaining = str;
    while (remaining.size() > 0 && (maxParts == 0 || parts.size() < maxParts)) {
        const auto pos = remaining.find(delim);
        if (pos == std::string_view::npos) {
            break;
        }
        parts.emplace_back(remaining.substr(0, pos));
        remaining = remaining.substr(pos + 1);
    }
    if (remaining.size() > 0) {
        if (parts.size() < maxParts) {
            parts.push_back(remaining);
        } else {
            // Append to last element (very hacky)
            const auto offset = parts.back().data() - str.data();
            parts.back() = str.substr(offset);
        }
    }
    return parts;
}

std::string_view trim(std::string_view str)
{
    static constexpr auto spaces = " \f\n\r\t\v";
    const auto start = str.find_first_not_of(spaces);
    const auto end = str.find_last_not_of(spaces);
    return str.substr(start, end - start + 1);
}
}

int parse(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<ParseArgs>(argc, argv).value();

    const auto numModes = int(args.regex.has_value()) + int(args.csv.has_value());
    if (numModes != 1) {
        std::cerr << "Please pass exactly one of --csv, --regex or --sccanf" << std::endl;
        return 1;
    }

    std::vector<Column> columns;
    for (const auto& col : args.columns) {
        columns.push_back(Column { col, Column::Type::String });
    }

    Output output(columns);

    std::function<std::vector<Value>(std::string_view line)> parseLine;

    if (args.regex) {
        std::regex regex(*args.regex);
        if (regex.mark_count() != columns.size()) {
            std::cerr << "Regular expression must have the same number of match groups ("
                      << regex.mark_count() << ") as there are columns " << columns.size()
                      << " specified" << std::endl;
            return 1;
        }
        parseLine = [regex = std::move(regex), &args](std::string_view line) {
            std::smatch m;
            const std::string lineStr = std::string(line);
            if (!std::regex_match(lineStr, m, regex)) {
                std::cerr << "Line does not match regex: " << line << std::endl;
                std::exit(1);
            }
            std::vector<Value> values;
            for (auto it = m.begin() + 1; it != m.end(); ++it) {
                values.push_back(it->str());
            }
            return values;
        };
    } else if (args.csv) {
        parseLine = [&columns, &args](std::string_view line) {
            std::vector<Value> values;
            const auto parts = split(line, *args.csv, columns.size());
            for (const auto& part : parts) {
                if (args.trim) {
                    values.push_back(std::string(trim(part)));
                } else {
                    values.push_back(std::string(part));
                }
            }
            return values;
        };
    } else {
        std::abort();
    }

    char readBuffer[4096];
    ssize_t num = 0;
    std::string lineBuffer;
    while ((num = ::read(STDIN_FILENO, readBuffer, sizeof(readBuffer))) > 0) {
        std::string_view readView(readBuffer, num);
        size_t delimPos;
        while ((delimPos = readView.find(*args.rowDelim)) != std::string_view::npos) {
            if (lineBuffer.size() > 0) {
                lineBuffer.append(readView.substr(0, delimPos));
                output.row(parseLine(lineBuffer));
                lineBuffer.clear();
            } else {
                output.row(parseLine(readView.substr(0, delimPos)));
                readView = readView.substr(delimPos + 1);
            }
        }
        if (readView.size() > 0) {
            lineBuffer.assign(readView);
        }
    }

    return 0;
}
