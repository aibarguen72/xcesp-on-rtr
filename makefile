# makefile - xcesp-on-rtr
# ObjectNode library: XCESP-ON-RTR (Ethernet/IP/VLAN, Static Routing, VRF/NS)

include PROJECT

CXX      := g++
XCESPPROC := ../xcespproc
CXXFLAGS := -std=c++17 -Wall -Wextra -MMD -MP \
            -I $(XCESPPROC)/src \
            -I $(XCESPPROC)/include \
            -DPRJNAME=\"$(PRJNAME)\" -DPRJVERSION=\"$(PRJVERSION)\"

SRCDIR   := src
BUILDDIR := build
LIBDIR   := lib
TARGET   := $(LIBDIR)/libon-rtr.a

# Override CXXFLAGS to include own source directories (must come after SRCDIR)
CXXFLAGS += -I $(SRCDIR) -I $(SRCDIR)/objects

SRCS     := $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/objects/*.cpp)
OBJS     := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)
-include $(DEPS)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS) | $(LIBDIR)
	ar rcs $@ $(OBJS)

# src/*.cpp → build/%.o
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# src/objects/*.cpp → build/objects/%.o
$(BUILDDIR)/objects/%.o: $(SRCDIR)/objects/%.cpp | $(BUILDDIR)/objects
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(LIBDIR) $(BUILDDIR):
	mkdir -p $@

$(BUILDDIR)/objects:
	mkdir -p $@

install: all

clean:
	rm -rf $(BUILDDIR) $(TARGET)
