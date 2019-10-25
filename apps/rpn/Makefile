TARGET = rpn

CPPFLAGS += -I.
CFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding
CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -fno-exceptions -ffreestanding -fno-rtti -std=c++2a
LDFLAGS += --entry main -z norelro --image-base 0x100000 --static

.PHONY: all
all: $(TARGET)

rpn: rpn.o Makefile
	ld.lld $(LDFLAGS) -o rpn rpn.o -lc -lc++

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
