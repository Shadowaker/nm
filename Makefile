NAME    = ft_nm
CC      = cc
CFLAGS  = -Wall -Wextra -Werror

SRCS    = main.c elf_load.c elf_validate.c elf_report.c
OBJS    = $(SRCS:.c=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

%.o: %.c ft_nm.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
