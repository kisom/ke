TARGET :=		ke
KE_VERSION :=		devel
DEST :=			$(HOME)/.local/bin/$(TARGET)

CFLAGS :=	-Wall -Wextra -pedantic -Wshadow -Werror -std=c99 -g
CFLAGS +=	-D_DEFAULT_SOURCE -D_XOPEN_SOURCE
CFLAGS +=	-fsanitize=address -fno-omit-frame-pointer

LDFLAGS :=	-fsanitize=address

all: $(TARGET) test.txt

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) $(LDFLAGS) main.c

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
