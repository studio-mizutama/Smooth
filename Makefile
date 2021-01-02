NAME = Effect
# where are our OFX headers
OFX_INC_DIR = -I../sdk/Examples/Headers -I../openfx/include

# by default we don't optimise
OPTIMISE = -g

# figure out our system specific variables
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
  OS_CXXFLAGS = -fvisibility=hidden
  OS_LDFLAGS = -bundle -fvisibility=hidden
  OS_BUNDLE_DIR = MacOS
else ifeq ($(UNAME), Linux)
  OS_CXXFLAGS = -fPIC -fvisibility=hidden
  OS_LDFLAGS = -shared -fvisibility=hidden
  OS_BUNDLE_DIR = Linux-x86-64
endif

# set up C++ flags
CXXFLAGS = $(OFX_INC_DIR) $(OPTIMISE) $(OS_CXXFLAGS)

.PHONY: clean install

# make our ofx bundle
$(NAME).ofx : $(NAME).o 8link.o downMode.o Lack.o upMode.o util.o
	$(CXX) $(OS_LDFLAGS) $(NAME).o 8link.o downMode.o Lack.o upMode.o util.o -o $@
	mkdir -p $@.bundle/Contents/$(OS_BUNDLE_DIR)/
	cp $@ $@.bundle/Contents/$(OS_BUNDLE_DIR)/

8link.o : 8link.cpp
	$(CXX) $(CXXFLAGS) -c 8link.cpp
downMode.o : downMode.cpp
	$(CXX) $(CXXFLAGS) -c downMode.cpp
Lack.o : Lack.cpp
	$(CXX) $(CXXFLAGS) -c Lack.cpp
upMode.o : upMode.cpp
	$(CXX) $(CXXFLAGS) -c upMode.cpp
util.o : util.cpp
	$(CXX) $(CXXFLAGS) -c util.cpp
$(NAME).o : $(NAME).cpp
	$(CXX) $(CXXFLAGS) -c $(NAME).cpp

# install it
install : $(NAME).ofx
	mkdir -p ../built_plugins
	cp -r $(NAME).ofx.bundle ../built_plugins

# clean it
clean :
	rm -rf $(NAME).ofx.bundle
	rm -rf $(NAME).ofx
	rm -rf *.o
