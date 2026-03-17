# makefile - xcesp-on-rtr
# ObjectNode library: XCESP-ON-RTR (Ethernet/IP/VLAN, Static Routing, VRF/NS)

include PROJECT

CXX      := g++
XCESPPROC := ../../xcespproc
CXXFLAGS := -std=c++17 -Wall -Wextra -MMD -MP \
            -I $(XCESPPROC)/src \
            -I $(XCESPPROC)/include \
            -DPRJNAME=\"$(PRJNAME)\" -DPRJVERSION=\"$(PRJVERSION)\"

SRCDIR   := src
BUILDDIR := build
LIBDIR   := lib
TARGET   := $(LIBDIR)/libon-rtr.a

SRCS     := $(wildcard $(SRCDIR)/*.cpp)
OBJS     := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)
-include $(DEPS)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS) | $(LIBDIR)
	ar rcs $@ $(OBJS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(LIBDIR) $(BUILDDIR):
	mkdir -p $@

install: all

clean:
	rm -rf $(BUILDDIR) $(TARGET)
