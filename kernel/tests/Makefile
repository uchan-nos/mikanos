CFLAGS   += -O2 -Wall -g
CPPFLAGS   += -O2 -Wall -g -std=c++17 -MMD -MP -MF $(@:.o=.d)

TARGET = tests
VPATH = ../
SRC = main.cpp tokenizer.cpp tokenizer_test.cpp
OBJS = $(SRC:.cpp=.o)
DEPS = $(SRC:.cpp=.d)

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o $(TARGET) $(DEPS)

$(TARGET): $(OBJS) Makefile  
	clang++ $(LDFLAGS) -o $@ $(OBJS) -fuse-ld=lld

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: test
test: $(TARGET)
	./$(TARGET)

-include $(DEPS)
.SECONDARY: $(OBJS)
