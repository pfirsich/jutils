#pragma once

#include "io.hpp"

std::optional<size_t> getColumnIndex(const std::vector<Column>& columns, const std::string& column);
std::string toString(const Value& val);
