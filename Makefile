CFLAGS = -g -MMD -Wall -pedantic -Werror -std=c11
CXXFLAGS = -g -MMD -Wall -pedantic -Werror -std=c++17
LIBS = -lm
CC = gcc
CXX = g++
OFILES = $(patsubst %.c,%.o,$(wildcard *.c)) $(patsubst %.cpp,%.o,$(wildcard *.cpp))
DFILES = $(patsubst %.c,%.d,$(wildcard *.c)) $(patsubst %.cpp,%.d,$(wildcard *.cpp))
HFILES = $(wildcard *.h *.hpp)
PROG = procsim
TARBALL = $(if $(USER),$(USER),gburdell3)-proj3.tar.gz

ifdef PROFILE
FAST=1
undefine DEBUG
CFLAGS += -pg
CXXFLAGS += -pg
LIBS += -pg
endif

ifdef SANITIZE
CFLAGS += -fsanitize=address
CXXFLAGS += -fsanitize=address
LIBS += -fsanitize=address
endif

ifdef DEBUG
CFLAGS += -DDEBUG
CXXFLAGS += -DDEBUG
endif

ifdef FAST
CFLAGS += -O2
CXXFLAGS += -O2
endif

.PHONY: all validate submit clean

all: $(PROG)

$(PROG): $(OFILES)
	$(CXX) -o $@ $^ $(LIBS)

%.o: %.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp $(HFILES)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

validate: $(PROG)
	@bash validate.sh

submit: clean
	tar --exclude=project3_v*.pdf -czhvf $(TARBALL) run.sh Makefile $(wildcard *.pdf *.cpp *.c *.hpp *.h)
	@echo
	@echo 'submission tarball written to' $(TARBALL)
	@echo 'please decompress it yourself and make sure it looks right!'

clean:
	rm -f $(TARBALL) $(PROG) $(OFILES) $(DFILES)

-include $(DFILES)

# if you're a student, ignore this
-include ta-rules.mk
