CC := cc
INC_DIR := ./include/
CFLAGS := -ggdb -fsanitize=address -Wall -Wextra -pedantic -I$(INC_DIR)
# CFLAGS := -O3 -Wall -Wextra -pedantic -I$(INC_DIR)
EXECUTABLE := ini

# These paths should always be in the form ./<path>/
SRC_DIR := ./src/
OBJ_DIR := ./build/

SOURCES := $(shell find $(SRC_DIR) -name '*.c')
OBJECTS := $(patsubst $(SRC_DIR)%.c,$(OBJ_DIR)%.o,$(SOURCES))

OBJ_DIRS := $(shell dirname $(OBJECTS) | sort -u)
$(shell mkdir -p $(OBJ_DIRS))

BASENAMES := $(patsubst $(SRC_DIR)%.c,%,$(SOURCES))

all: $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $(EXECUTABLE)

define define_target
$(OBJ_DIR)$(1).o: $(SRC_DIR)$(1).c
	$(CC) $(CFLAGS) -o $(OBJ_DIR)$(1).o -c $(SRC_DIR)$(1).c
endef

clean:
	rm -rf $(OBJ_DIR) $(EXECUTABLE)

$(foreach i,$(BASENAMES),$(eval $(call define_target,$i)))


