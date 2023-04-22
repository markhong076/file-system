# Target library
lib 	:= libfs.a
objs    := disk.o fs.o

CC		:= gcc
# CFLAGS 	:= -Wall -Wextra -Werror -MMD
CFLAGS 	+= -g

ifneq ($(V),1)
Q = @
endif

all: $(lib)

# Dep tracking *must* be below the 'all' rule
deps = $(patsubst %.o,%.d,$(objs))
-include $(deps)

$(lib): $(objs)
	@echo "ar rcs $@"
	$(Q)ar rcs $@ $^

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "clean"
	$(Q)rm -f $(lib) $(objs) $(deps)
