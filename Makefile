TARGET :=		ke
KE_VERSION :=		devel
DEST :=			$(HOME)/.local/bin/$(TARGET)

CFLAGS :=	-Wall -Wextra -pedantic -Wshadow -Werror -std=c99 -g
CFLAGS +=	-Wno-unused-result
CFLAGS +=	-D_DEFAULT_SOURCE -D_XOPEN_SOURCE
CFLAGS +=	-fsanitize=address -fno-omit-frame-pointer 

LDFLAGS :=	-fsanitize=address

all: $(TARGET) test.txt

SRCS := main.c abuf.c term.c buffer.c

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

.PHONY: install
#install: $(TARGET) 
install:
	cp $(TARGET) $(DEST)

clean:
	rm -f $(TARGET)
	rm -f asan.log*

.PHONY: test.txt
test.txt:
	cp test.txt.bak $@

.PHONY: gdb
gdb:
	@test -f $(TARGET).pid || (echo "error: $(TARGET).pid not found" >&2; exit 1)
	@gdb -p $$(cat $(TARGET).pid | tr -d ' \t\n\r') ./$(TARGET)
