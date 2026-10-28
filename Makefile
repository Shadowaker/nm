NAME        = ft_nm
CC          = cc
CFLAGS      = -Wall -Wextra -Werror

SRCS        = main.c elf_load.c elf_validate.c elf_report.c elf_symbols.c elf_archive.c
HEADER      = ft_nm.h
OBJ_DIR     = objs
OBJS        = $(addprefix $(OBJ_DIR)/, $(SRCS:.c=.o))

GREEN       = \033[32m
BLUE        = \033[34m
RED         = \033[31m
BOLD        = \033[1m
RESET       = \033[0m

# The bar only counts the sources that this run will really compile
BAR_WIDTH   = 30
TODO        := $(shell for s in $(SRCS); do o=$(OBJ_DIR)/$${s%.c}.o; \
                if [ ! -f $$o ] || [ $$s -nt $$o ] || [ $(HEADER) -nt $$o ]; then echo $$s; fi; done)
TOTAL       := $(words $(TODO))
DONE        :=

# $(call progress,file) counts one more file and redraws the bar, showing
# the file just compiled, or "done" after the last one
define progress
$(eval DONE += x)total=$(TOTAL); done=$(words $(DONE)); [ $$total -ge $$done ] || total=$$done; \
    fill=$$((done * $(BAR_WIDTH) / total)); \
    printf "\r$(BLUE)Compiling$(RESET) [%s%s] %3d%% $(BOLD)%-16s$(RESET)" \
        "$$(printf '%*s' $$fill '' | sed 's/ /█/g')" \
        "$$(printf '%*s' $$(($(BAR_WIDTH) - fill)) '' | sed 's/ /░/g')" \
        $$((done * 100 / total)) $(if $(filter $(TOTAL),$(words $(DONE))),done,$(1))
endef

all: $(NAME)

$(NAME): $(OBJS)
	@$(CC) $(CFLAGS) $(OBJS) -o $(NAME)
	@printf "$(if $(DONE),\n)$(GREEN)$(BOLD)✔ $(NAME) ready$(RESET)\n"

# The compiler's output is only shown when it fails
$(OBJ_DIR)/%.o: %.c $(HEADER) | $(OBJ_DIR)
	@$(CC) $(CFLAGS) -c $< -o $@ 2> $@.log \
        || { printf "\n$(RED)$(BOLD)✘ $<$(RESET)\n"; cat $@.log; rm -f $@.log; exit 1; }
	@rm -f $@.log
	@$(call progress,$<)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	@rm -rf $(OBJ_DIR)
	@printf "$(RED)✘ removed $(OBJ_DIR)/$(RESET)\n"

fclean: clean
	@rm -f $(NAME)
	@printf "$(RED)✘ removed $(NAME)$(RESET)\n"

re: fclean
	@$(MAKE) --no-print-directory all

.PHONY: all clean fclean re
