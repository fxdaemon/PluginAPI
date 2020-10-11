###################################################
#
# Makefile
#
# Build: libForexApi.so
#
###################################################

buildtype := release

TARGET = libForexApi.so
CXX = g++
SRCEXT = cpp

INCLUDES =

LIBS = -Llib/linux/ 

CXXFLAGS = -std=c++11 -pthread -Wall -Wextra -fPIC -DSI_NO_CONVERSION -DSI_Case=SI_GenericCase -DSI_NoCase=SI_GenericNoCase
ifeq ($(buildtype), release)
  CXXFLAGS += -O3
else ifeq ($(buildtype), debug)
  CXXFLAGS += -O0 -g
else
  $(error buildtype must be release, debug)
endif

LDFLAGS = -lPluginProxy \
	-lForexConnect -lfxtp -lhttplib -lfxmsg -lpdas -llog4cplus -lgsexpat -lgstool3 \
	-shared -pthread


OUTDIR = $(buildtype)
SRCS = $(shell find src/ -name *.$(SRCEXT))
OBJS = $(SRCS:%.$(SRCEXT)=$(OUTDIR)/%.o)
DEPS = $(SRCS:%.$(SRCEXT)=$(OUTDIR)/%.d)
PROG = $(OUTDIR)/$(TARGET)

.PHONY: install clean distclean

all: $(PROG)

-include $(DEPS)

$(PROG): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OUTDIR)/%.o:%.$(SRCEXT)
	@if [ ! -e `dirname $@` ]; then mkdir -p `dirname $@`; fi
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ -c -MMD -MP -MF $(@:%.o=%.d) $<

clean:
	rm -rf $(OUTDIR)/*

distclean:
	rm -rf $(OUTDIR)

