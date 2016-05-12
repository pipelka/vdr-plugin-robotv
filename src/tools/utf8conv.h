#ifndef ROBOTV_UTF8CONF_H
#define ROBOTV_UTF8CONF_H

#include "utf8.h"
#include "vdr/tools.h"

class Utf8Conv : protected cCharSetConv {
public:

    Utf8Conv();

    std::string convert(const std::string& from);

    std::string convert(const char* from);

};

#endif // ROBOTV_UTF8CONF_H
