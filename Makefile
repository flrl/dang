TARGET=dang
all : $(TARGET)

CSRCS := $(wildcard *.c)
CXXSRCS := $(wildcard *.cpp)
SRCS := $(CSRCS) $(CXXSRCS)
OBJS := $(CSRCS:.c=.o) $(CXXSRCS:.cpp=.o) 
DEPS := $(CSRCS:.c=.d) $(CXXSRCS:.cpp=.d)
GENS := floatptr_t.h instruction_table.h instruction_table.c

CFLAGS += --std=c99 -g -Wall -Wno-unknown-pragmas
CXXFLAGS += -g -Wall -Wno-unknown-pragmas
LDFLAGS += -lm

MACHINE := $(shell uname -s)
-include $(MACHINE).mk

.PHONY : all clean depends realclean what

floatptr_t.h : make_floatptr_h.sh
	env CC=$(CC) sh make_floatptr_h.sh

instruction_table.h instruction_table.c : make_instruction_table.pl bytecode.h
	perl make_instruction_table.pl instruction_table < bytecode.h

$(TARGET) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

$(DEPS) : $(GENS)

clean :
	$(RM) $(OBJS) $(TARGET) core

depends : $(DEPS)

realclean : clean
	$(RM) $(DEPS) $(GENS)

define DEPEND_template
 $(1)_SRC := $(filter $(1).%,$(SRCS))
 $(1).d : $$($(1)_SRC)
	$$(CC) $$(CFLAGS) -MM -MF $(1).d -MT $(1).o $$($(1)_SRC)
endef
$(foreach depend,$(basename $(DEPS)),$(eval $(call DEPEND_template,$(depend))))

-include $(DEPS)
