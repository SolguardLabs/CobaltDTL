#pragma once

#include <string>
#include <string_view>

namespace cobaltdtl {

std::string sha256(std::string_view input);

}  // namespace cobaltdtl
