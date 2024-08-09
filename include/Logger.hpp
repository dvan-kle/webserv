#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "Libaries.hpp"
#include "Colors.hpp"

class Logger {
    public:
        static void Error(const std::string &message) {
            std::cout << BBLACK << "[" << ON_RED << BRED << " ERROR " << RESET << BBLACK << "] " << BRED << message << RESET << std::endl;
        }

        static void Warning(const std::string &message) {
            std::cout << BBLACK << "[" << ON_YELLOW << BYELLOW << " WARNING " << RESET << BBLACK << "] " << BYELLOW << message << RESET << std::endl;
        }

        static void Info(const std::string &message) {
            std::cout << BBLACK << "[" << ON_CYAN << BCYAN << " INFO " << RESET << BBLACK << "] " << BCYAN << message << RESET << std::endl;
        }

        static void Success(const std::string &message) {
            std::cout << BBLACK << "[" << ON_GREEN << BGREEN << " SUCCESS " << RESET << BBLACK << "] " << BGREEN << message << RESET << std::endl;
        }
};

#endif
