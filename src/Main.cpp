#include "../include/JsonParser.hpp"
#include "../include/Server.hpp"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Usage: " + std::string(argv[0]) + " <config_file>" << std::endl;
        return 1;
    }

    parseConfig(argc, argv);

    Server server(8080);

    return 0;
}
