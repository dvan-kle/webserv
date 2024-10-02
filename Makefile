NAME = webserv
CC = c++
CFLAGS = -std=c++20 -g
BUILD_DIR = build
SRC_DIR = src
INC_DIR = include

SOURCES = \
	src/AutoIndex.cpp \
	src/CGI.cpp \
	src/Delete.cpp \
	src/Errors.cpp \
	src/Header.cpp \
	src/JsonParser.cpp \
	src/Main.cpp \
	src/Post.cpp \
	src/Redirect.cpp \
	src/Request.cpp \
	src/Server.cpp \
	src/Utils.cpp \
	src/Get.cpp \

OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

RED = \033[1;31m
GREEN = \033[1;32m1
YELLOW = \033[1;33m
BLUE = \033[1;34m
RESET = \033[0m

all: $(BUILD_DIR) $(NAME)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(NAME): $(OBJECTS)
	@echo "$(YELLOW)Linking $(NAME)...$(RESET)"
	@$(CC) $(CFLAGS) -o $(NAME) $(OBJECTS)
	@echo "$(GREEN)$(NAME) compiled successfully!$(RESET)"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@
	@echo "$(BLUE)Compiling $< ...$(RESET)"

clean:
	@rm -rf $(BUILD_DIR)

fclean: clean
	@rm -f $(NAME)

re: fclean all