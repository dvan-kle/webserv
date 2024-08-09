#include "../include/Libaries.hpp"
#include "../include/Colors.hpp"
#include "../include/Logger.hpp"

void Logger::Error(const std::string &message) {
    std::cout << BBLACK << "[" << ON_RED << BRED << " ERROR " << RESET << BBLACK << "] " << BRED << message << RESET << std::endl;
}

void Logger::Warning(const std::string &message) {
    std::cout << BBLACK << "[" << ON_YELLOW << BYELLOW << " WARNING " << RESET << BBLACK << "] " << BYELLOW << message << RESET << std::endl;
}

void Logger::Info(const std::string &message) {
    std::cout << BBLACK << "[" << ON_CYAN << BCYAN << " INFO " << RESET << BBLACK << "] " << BCYAN << message << RESET << std::endl;
}

void Logger::Success(const std::string &message) {
    std::cout << BBLACK << "[" << ON_GREEN << BGREEN << " SUCCESS " << RESET << BBLACK << "] " << BGREEN << message << RESET << std::endl;
}
