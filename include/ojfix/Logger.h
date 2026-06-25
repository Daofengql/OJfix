#pragma once

#include <string>

namespace ojfix {

void WriteInfo(const std::string& message);
void WriteError(const std::string& message);
std::string CurrentTimeText();

} // namespace ojfix
