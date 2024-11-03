CC := cc
LIB := ar
INC_DIR := ./include/
CFLAGS := -ggdb -fsanitize=address -Wall -Wextra -pedantic -I$(INC_DIR)
# CFLAGS := -O3 -Wall -Wextra -pedantic -I$(INC_DIR)
OUT := ini.a

TEST_SOURCES := test.c
TEST_EXECUTABLE := testini

# These paths should always be in the form ./<path>/
SRC_DIR := ./src/
OBJ_DIR := ./build/

SOURCES := $(shell find $(SRC_DIR) -name '*.c')
OBJECTS := $(patsubst $(SRC_DIR)%.c,$(OBJ_DIR)%.o,$(SOURCES))

OBJ_DIRS := $(shell dirname $(OBJECTS) | sort -u)
$(shell mkdir -p $(OBJ_DIRS))

BASENAMES := $(patsubst $(SRC_DIR)%.c,%,$(SOURCES))

all: library

library: $(OBJECTS)
	$(LIB) rcs $(OUT) $^

define define_target
$(OBJ_DIR)$(1).o: $(SRC_DIR)$(1).c
	$(CC) $(CFLAGS) -o $(OBJ_DIR)$(1).o -c $(SRC_DIR)$(1).c
endef

test: test.c library
	$(CC) $(CFLAGS) -o $(TEST_EXECUTABLE) $(TEST_SOURCES) -L. -l:$(OUT)

main: main.c $(OUT)
	$(CC) $(CFLAGS) -o main main.c -L. -l:$(OUT)

clean:
	rm -rf $(OBJ_DIR) $(EXECUTABLE) $(TEST_EXECUTABLE)

$(foreach i,$(BASENAMES),$(eval $(call define_target,$i)))


