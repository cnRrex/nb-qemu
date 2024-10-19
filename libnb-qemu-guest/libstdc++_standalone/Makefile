CURRDIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

$(BUILDDIR)/libstdc++.so: $(CURRDIR)/new.cpp $(CURRDIR)/__cxa_pure_virtual.cpp $(CURRDIR)/__cxa_guard.cpp
	clang $(CROSSFLAGS) -fno-exceptions $(LDFLAGS) $(CFLAGS) $(LDLIBS) $(INCLUDES) -I$(CURRDIR)/include -o $@ $^
