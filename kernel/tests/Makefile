CFLAGS   += -O2 -Wall -g
CPPFLAGS   += -O2 -Wall -g -std=c++17

TARGET = tests
OBJS = main.o tokenizer.o tokenizer_test.o

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o $(TARGET)

$(TARGET): $(OBJS) Makefile  
	clang++ $(LDFLAGS) -o $@ $(OBJS) -fuse-ld=lld

tokenizer.o: ../tokenizer.cpp Makefile
	clang++ $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: test
test: $(TARGET)
	./$(TARGET)

