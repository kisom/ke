TARGET :=		ke
KE_VERSION :=		devel
DEST :=			$(HOME)/.local/bin/$(TARGET)

CC :=		gcc
CXX :=		g++

CFLAGS :=	-Wall -Wextra -pedantic -Wshadow -Werror -std=c99 -g
CFLAGS +=	-Wno-unused-result
CFLAGS +=	-D_DEFAULT_SOURCE -D_XOPEN_SOURCE
CFLAGS +=	-fsanitize=address -fno-omit-frame-pointer 

CXXFLAGS :=	-Wall -Wextra -pedantic -Wshadow -Werror -std=c++17 -g
CXXFLAGS +=	-Wno-unused-result
CXXFLAGS +=	-D_DEFAULT_SOURCE -D_XOPEN_SOURCE
CXXFLAGS +=	-DKE_VERSION='"ke $(KE_VERSION)"'
CXXFLAGS +=	-fsanitize=address -fno-omit-frame-pointer

LDFLAGS :=	-fsanitize=address

SOURCES :=	main.cc abuf.cc erow.cc
OBJECTS :=	main.o abuf.o erow.o

all: $(TARGET) test.txt

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

main.o: main.cc ke_constants.h
	$(CXX) $(CXXFLAGS) -c main.cc

abuf.o: abuf.cc abuf.h
	$(CXX) $(CXXFLAGS) -c abuf.cc

erow.o: erow.cc erow.h
	$(CXX) $(CXXFLAGS) -c erow.cc

.PHONY: install
#install: $(TARGET) 
install:
	cp $(TARGET) $(DEST)

clean:
	rm -f $(TARGET)
	rm -f $(OBJECTS)
	rm -f asan.log*

.PHONY: test.txt
test.txt:
	cp test.txt.bak $@
