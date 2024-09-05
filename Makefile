NAME	:= webserv
CFLAGS	:= -Wextra -Wall -Werror -g

HEADERS	:= -I ./include -I $(LIBMLX)/include 

SRCS := \
	src/Main.cpp \
	src/Logger.cpp \
	src/LocationConfig.cpp \
	src/ServerConfig.cpp \
	src/ConfigParser.cpp \
	src/Server.cpp \
	src/Request.cpp \
	src/Errors.cpp \
	src/Post.cpp \

OBJS	:= $(SRCS:%.c=objs/%.o)
OBJS_DIR = objs/

RED = \033[1;31m
GREEN = \033[1;32m
YELLOW = \033[1;33m
BLUE = \033[1;34m
RESET = \033[0m

all: $(NAME)

$(NAME): $(OBJS)
	@echo "$(YELLOW)Compiling $(NAME)...$(RESET)"
	c++ -std=c++20 $(OBJS) -o $(NAME)
	@echo "$(GREEN)$(NAME) compiled successfully!$(RESET)"

$(OBJS_DIR)%.o: %.c
	@echo "$(BLUE)Creating $(NAME) object files...$(RESET)"
	mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(HEADERS) -c $< -o $@

clean:
	@rm -rf objs
	@rm -rf $(LIBMLX)/build

fclean: clean
	@rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re
