TARGET = test.run
OBJS = $(shell make -f print-objs --quiet print-objs)
EXCLUDE_OBJS = main.o logger.o newlib_support.o

OBJROOT = $(PWD)
OBJS := $(addprefix $(OBJROOT)/,$(filter-out $(EXCLUDE_OBJS),$(OBJS)))
OBJS := $(OBJS) main.o logger.o test_memory_manager.o
DEPENDS = $(join $(dir $(OBJS)),$(addprefix .,$(notdir $(OBJS:.o=.d))))

CPPFLAGS = -I. -I..
CFLAGS = -O2 -Wall -g -fPIC
CXXFLAGS = -O2 -Wall -g -fPIC -std=c++2a

CC = clang
CXX = clang++

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	find . -name '*.o' -delete
	rm hankaku.bin

.PHONY: run
run: test.run
	./test.run

test.run: $(OBJS)
	$(CXX) -o test.run $(OBJS) -lCppUTest -lCppUTestExt -lpthread

$(OBJROOT)/%.o: ../%.cpp Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJROOT)/.%.d: ../%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MM $< > $@
	$(eval OBJ = $(<:.cpp=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

.%.d: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MM $< > $@
	$(eval OBJ = $(<:.cpp=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

$(OBJROOT)/%.o: ../%.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJROOT)/.%.d: ../%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MM $< > $@
	$(eval OBJ = $(<:.c=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

$(OBJROOT)/%.o: ../%.asm Makefile
	nasm -f elf64 -o $@ $<

hankaku.bin: ../hankaku.txt
	../../tools/makefont.py -o $@ $<

$(OBJROOT)/hankaku.o: hankaku.bin
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $< $@

.%.d: %.bin
	touch $@

.PHONY: depends
depends:
	$(MAKE) $(DEPENDS)

-include $(DEPENDS)
