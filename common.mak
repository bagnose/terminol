INSTALLDIR  ?= ~/local/terminol
VERBOSE     ?= false
VERSION     ?= $(shell cd src && git log -1 --format='%cd.%h' --date=short | tr -d -)

PCMODULES   := pangocairo pango cairo fontconfig xcb-keysyms xcb-icccm xcb-ewmh xcb-util xkbcommon

ifeq ($(shell pkg-config $(PCMODULES) && echo installed),)
  $(error Missing packages from: $(PCMODULES))
endif

PKG_CFLAGS  := $(shell pkg-config --cflags $(PCMODULES))
PKG_LDFLAGS := $(shell pkg-config --libs   $(PCMODULES))

CPPFLAGS    := -iquotesrc -DVERSION=\"$(VERSION)\"
CXXFLAGS    := -fpic -fno-rtti -pedantic -std=c++11
WFLAGS      := -Werror -Wextra -Wall -Wno-long-long -Wundef \
               -Wredundant-decls -Wshadow -Wsign-compare \
               -Wmissing-field-initializers -Wno-format-zero-length \
               -Wno-unused-function -Woverloaded-virtual -Wsign-promo \
               -Wctor-dtor-privacy -Wnon-virtual-dtor
AR          := ar
ARFLAGS     := csr
LDFLAGS     :=

# XXX global kludge (until we get the argument working):
CXXFLAGS += $(PKG_CFLAGS)

ifeq ($(COMPILER),gnu)
  CXX := g++
else ifeq ($(COMPILER),clang)
  CXX := clang++
else
  $(error Unrecognised COMPILER: $(COMPILER))
endif

ifeq ($(MODE),release)
  CPPFLAGS += -DDEBUG=0 -DNDEBUG
  WFLAGS   += -Wno-unused-parameter
  ifeq ($(COMPILER),gnu)
    CXXFLAGS += -Os
  else
    CXXFLAGS += -Oz
    #CXXFLAGS += -O4
    #LDFLAGS  += -O4
  endif
else ifeq ($(MODE),debug)
  CPPFLAGS += -DDEBUG=1
  CXXFLAGS += -g
  ifeq ($(COMPILER),gnu)
    CXXFLAGS += -Og
  else
    CXXFLAGS += -O1
  endif
else ifeq ($(MODE),coverage)
  CPPFLAGS += -DDEBUG=1
  CXXFLAGS += -g --coverage
  LDFLAGS  += --coverage
  ifeq ($(COMPILER),gnu)
    CXXFLAGS += -Og
  else
    $(error Coverage is only support by gnu compiler)
  endif
else
  $(error Unrecognised MODE: $(MODE))
endif

ifeq ($(VERBOSE),false)
  V := @
else ifeq ($(VERBOSE),true)
  V :=
else
  $(error Unrecognised VERBOSE: $(VERBOSE))
endif

all:

.PHONY: clean

info:
	@echo 'CPPFLAGS: $(CPPFLAGS)'
	@echo 'CXXFLAGS: $(CXXFLAGS)'
	@echo 'LDFLAGS:  $(LDFLAGS)'

install: all
	install -D dist/bin/terminol  $(INSTALLDIR)/bin/terminol
	install -D dist/bin/terminols $(INSTALLDIR)/bin/terminols
	install -D dist/bin/terminolc $(INSTALLDIR)/bin/terminolc

clean:
	rm -rf obj priv dist

ifeq ($(MODE),coverage)
reset-coverage:
	lcov --directory . --zerocounters

coverage: all
	dist/bin/terminol
	lcov --directory . --capture --output-file app.info > /dev/null 2>&1
	genhtml --output-directory output app.info > /dev/null 2>&1
	chromium output/index.html
endif

obj/%.o: src/%.cxx
ifeq ($(VERBOSE),false)
	@echo ' [CXX] $(@F)'
endif
	$(V)$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WFLAGS) -c $< -o $@ -DVERSION=\"$(VERSION)\" -MMD -MF $(patsubst %.o,%.dep,$@)

# $(1) stem
# $(2) name
# $(3) directory
# $(4) sources
# $(5) CXXFLAGS
define LIBRARY
$(1)_SRC := $$(addprefix $(3)/src/,$(4))
$(1)_OBJ := $$(addprefix obj/$(3)/,$$(patsubst %.cxx,%.o,$(4)))
$(1)_DEP := $$(patsubst %.o,%.dep,$$($(1)_OBJ))
$(1)_LIB := $$(addprefix obj/,lib$(2)-s.a)

-include $$($(1)_DEP)

ifeq (,$(findstring obj/$(3),$(DIRS_DONE)))
  DIRS_DONE += obj/$(3)
  obj/$(3):
  ifeq ($(VERBOSE),false)
	@echo ' [DIR] $$@'
  endif
	$(V)mkdir -p $$@
endif

$$($(1)_OBJ): | obj/$(3)

$$($(1)_LIB): $$($(1)_OBJ)
ifeq ($(VERBOSE),false)
	@echo ' [LIB] $$(@F)'
endif
	$(V)$(AR) $(ARFLAGS) $$@ $$($(1)_OBJ)
endef

# $(1) stem
# $(2) name
# $(3) directory
# $(4) sources
# $(5) CXXFLAGS
# $(6) libraries
# $(7) LDFLAGS
define TEST
$(1)_SRC  := $$(addprefix $(3)/src/,$(4))
$(1)_OBJ  := $$(addprefix obj/$(3)/,$$(patsubst %.cxx,%.o,$(4)))
$(1)_DEP  := $$(patsubst %.o,%.dep,$$($(1)_OBJ))
$(1)_LIB  := $$(addsuffix -s.a,$$(addprefix obj/lib,$(6)))
$(1)_DIR  := priv/$(3)
$(1)_TEST := $$(addprefix $$($(1)_DIR)/,$(2))
$(1)_OUT  := $$($(1)_TEST).out
$(1)_PASS := $$($(1)_TEST).pass

-include $$($(1)_DEP)

ifeq (,$$(findstring $$($(1)_DIR),$(DIRS_DONE)))
DIRS_DONE += $$($(1)_DIR)
$$($(1)_DIR):
ifeq ($(VERBOSE),false)
	@echo ' [DIR] $$@'
endif
	$(V)mkdir -p $$@
endif

$$($(1)_OBJ): | obj/$(3)

$$($(1)_TEST): $$($(1)_OBJ) $$($(1)_LIB) | $$($(1)_DIR)
ifeq ($(VERBOSE),false)
	@echo ' [TST] $$(@F)'
endif
	$(V)$(CXX) $(LDFLAGS) -o $$@ $$($(1)_OBJ) $$($(1)_LIB) $(7)

$$($(1)_PASS): $$($(1)_TEST)
	@echo ' [RUN] $$(<F)'
	@$$($(1)_TEST) > $$($(1)_OUT) 2>&1 && touch $$@ || (rm -f $$@ && echo "Test failed '$(2)'." && cat $$($(1)_OUT))

all: $$($(1)_PASS)
endef

# $(1) stem
# $(2) name
# $(3) directory
# $(4) sources
# $(5) CXXFLAGS
# $(6) libraries
# $(7) LDFLAGS
define EXE
$(1)_SRC := $$(addprefix $(3)/src/,$(4))
$(1)_OBJ := $$(addprefix obj/$(3)/,$$(patsubst %.cxx,%.o,$(4)))
$(1)_DEP := $$(patsubst %.o,%.dep,$$($(1)_OBJ))
$(1)_LIB := $$(addsuffix -s.a,$$(addprefix obj/lib,$(6)))
$(1)_DIR := dist/bin
$(1)_EXE := $$(addprefix $$($(1)_DIR)/,$(2))

-include $$($(1)_DEP)

ifeq (,$$(findstring $$($(1)_DIR),$(DIRS_DONE)))
DIRS_DONE += $$($(1)_DIR)
$$($(1)_DIR):
ifeq ($(VERBOSE),false)
	@echo ' [DIR] $$@'
endif
	$(V)mkdir -p $$@
endif

$$($(1)_OBJ): | obj/$(3)

$$($(1)_EXE): $$($(1)_OBJ) $$($(1)_LIB) | $$($(1)_DIR)
ifeq ($(VERBOSE),false)
	@echo ' [EXE] $$($(1)_EXE)'
endif
	$(V)$(CXX) $(LDFLAGS) -o $$@ $$($(1)_OBJ) $$($(1)_LIB) $(7)

all: $$($(1)_EXE)
endef

#
# SUPPORT
#

$(eval $(call LIBRARY,SUPPORT,terminol-support,terminol/support,conv.cxx debug.cxx escape.cxx pattern.cxx time.cxx,))

$(eval $(call TEST,TEST_SUPPORT,test-support,terminol/support,test_support.cxx,,terminol-support))

#
# COMMON
#

$(eval $(call LIBRARY,COMMON,terminol-common,terminol/common,ascii.cxx bit_sets.cxx buffer.cxx config.cxx data_types.cxx deduper.cxx enums.cxx key_map.cxx parser.cxx terminal.cxx tty.cxx utf8.cxx vt_state_machine.cxx,))

$(eval $(call TEST,PARSER,test-parser,terminol/common,test_parser.cxx,,terminol-common terminol-support))

$(eval $(call TEST,UTF8,test-utf8,terminol/common,test_utf8.cxx,,terminol-common terminol-support))

$(eval $(call EXE,ABUSE,abuse,terminol/common,abuse.cxx,,terminol-common terminol-support,))

$(eval $(call EXE,SEQUENCER,sequencer,terminol/common,sequencer.cxx,,terminol-common terminol-support,))

$(eval $(call EXE,STYLES,styles,terminol/common,styles.cxx,,terminol-common terminol-support,))

$(eval $(call EXE,DROPPINGS,droppings,terminol/common,droppings.cxx,,terminol-common terminol-support,))

#
# XCB
#

$(eval $(call LIBRARY,XCB,terminol-xcb,terminol/xcb,basics.cxx color_set.cxx font_manager.cxx font_set.cxx window.cxx,$(PKG_CFLAGS)))

$(eval $(call EXE,TERMINOL,terminol,terminol/xcb,terminol.cxx,$(PKG_CFLAGS),terminol-xcb terminol-common terminol-support,$(PKG_LDFLAGS) -lutil))

$(eval $(call EXE,TERMINOLS,terminols,terminol/xcb,terminols.cxx,$(PKG_CFLAGS),terminol-xcb terminol-common terminol-support,$(PKG_LDFLAGS) -lutil))

$(eval $(call EXE,TERMINOLC,terminolc,terminol/xcb,terminolc.cxx,,terminol-common terminol-support,))
