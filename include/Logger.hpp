#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "Libaries.hpp"
#include "Colors.hpp"

class Logger {
    public:
        static void Error(const std::string &message);
        static void Warning(const std::string &message);
        static void Info(const std::string &message);
        static void Success(const std::string &message);
};

#endif
