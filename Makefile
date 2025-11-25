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

SOURCES :=	main.cc abuf.cc erow.cc terminal.cc input_handler.cc display.cc file_io.cc killring.cc
OBJECTS :=	main.o abuf.o erow.o terminal.o input_handler.o display.o file_io.o killring.o

all: $(TARGET) test.txt

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

main.o: main.cc ke_constants.h
	$(CXX) $(CXXFLAGS) -c main.cc

abuf.o: abuf.cc abuf.h
	$(CXX) $(CXXFLAGS) -c abuf.cc

erow.o: erow.cc erow.h
	$(CXX) $(CXXFLAGS) -c erow.cc

terminal.o: terminal.cc terminal.h
	$(CXX) $(CXXFLAGS) -c terminal.cc

input_handler.o: input_handler.cc input_handler.h ke_constants.h
	$(CXX) $(CXXFLAGS) -c input_handler.cc

display.o: display.cc display.h ke_constants.h
	$(CXX) $(CXXFLAGS) -c display.cc

file_io.o: file_io.cc file_io.h
	$(CXX) $(CXXFLAGS) -c file_io.cc

killring.o: killring.cc killring.h
	$(CXX) $(CXXFLAGS) -c killring.cc

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
