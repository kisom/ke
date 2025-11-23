TARGET :=		ke
KE_VERSION :=	devel


CFLAGS :=	-Wall -Wextra -pedantic -Wshadow -Werror -std=c99 -g
CFLAGS +=	-D_DEFAULT_SOURCE -D_XOPEN_SOURCE
CFLAGS +=	-fsanitize=address -fno-omit-frame-pointer

LDFLAGS :=	-fsanitize=address

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) $(LDFLAGS) main.c