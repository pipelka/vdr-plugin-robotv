#include "utf8conv.h"

Utf8Conv::Utf8Conv() : cCharSetConv(nullptr, "UTF-8") {
}

std::string Utf8Conv::convert(const std::string& from) {
    return convert(from.c_str());
}

std::string Utf8Conv::convert(const char* from) {
    std::string text = cCharSetConv::Convert(from);
    std::string result;

    utf8::replace_invalid(text.begin(), text.end(), std::back_inserter(result));

    return result;
}
