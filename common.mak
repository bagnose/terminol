INSTALLDIR  ?= ~/local/terminol
VERBOSE     ?= false
VERSION     ?= $(shell git --git-dir=src/.git log -1 --format='%cd.%h' --date=short | tr -d -)
BROWSER     ?= chromium

PCMODULES   := pangocairo pango cairo fontconfig xcb-keysyms xcb-icccm xcb-ewmh xcb-util xkbcommon

ifeq ($(shell pkg-config $(PCMODULES) && echo installed),)
  $(error Missing packages from: $(PCMODULES))
endif

PKG_CFLAGS  := $(shell pkg-config --cflags $(PCMODULES))
PKG_LDFLAGS := $(shell pkg-config --libs   $(PCMODULES))

CPPFLAGS    := -iquotesrc -DVERSION=\"$(VERSION)\"
CXXFLAGS    := -fpic -fno-rtti -pedantic -std=c++11
WFLAGS      := -Werror -Wextra -Wall -Wno-long-long -Wundef           \
               -Wredundant-decls -Wshadow -Wsign-compare              \
               -Wmissing-field-initializers -Wno-format-zero-length   \
               -Wno-unused-function -Woverloaded-virtual -Wsign-promo \
               -Wctor-dtor-privacy -Wnon-virtual-dtor
AR          := ar
ARFLAGS     := csr
LDFLAGS     :=

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

.PHONY: all info install clean

all:

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
.PHONY: reset-coverage report-coverage

reset-coverage:
	lcov --directory obj --zerocounters

report-coverage:
	lcov --directory obj --capture --output-file priv/coverage.trace > /dev/null 2>&1
	genhtml --output-directory priv/coverage-report priv/coverage.trace > /dev/null 2>&1
	$(BROWSER) priv/coverage-report/index.html
endif

obj/%.o: src/%.cxx
ifeq ($(VERBOSE),false)
	@echo ' [CXX] $(@F)'
endif
	$(V)$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WFLAGS) -c $< -o $@ -MMD -MF $(patsubst %.o,%.dep,$@) $($(<)_CXXFLAGS) -DVERSION=\"$(VERSION)\"

# $(1) directory
# $(2) sources
# $(3) CXXFLAGS
define LIBRARY
$(1)_SRC := $$(addprefix src/$(1)/,$(2))
$(1)_OBJ := $$(addprefix obj/$(1)/,$$(patsubst %.cxx,%.o,$(2)))
$(1)_DEP := $$(patsubst %.o,%.dep,$$($(1)_OBJ))
$(1)_LIB := $$(addprefix obj/,lib$$(subst /,-,$(1))-s.a)

$$(foreach SRC,$$($(1)_SRC),$$(eval $$(SRC)_CXXFLAGS := $(3)))

-include $$($(1)_DEP)

ifeq (,$(findstring obj/$(1),$(DIRS_DONE)))
DIRS_DONE += obj/$(1)
obj/$(1):
ifeq ($(VERBOSE),false)
	@echo ' [DIR] $$@'
endif
	$(V)mkdir -p $$@
endif

$$($(1)_OBJ): | obj/$(1)

$$($(1)_LIB): $$($(1)_OBJ)
ifeq ($(VERBOSE),false)
	@echo ' [LIB] $$(@F)'
endif
	$(V)$(AR) $(ARFLAGS) $$@ $$($(1)_OBJ)
endef

# $(1) path
# $(2) sources
# $(3) CXXFLAGS
# $(4) libraries (directories)
# $(5) LDFLAGS
define TEST
$(1)_SDIR := $$(patsubst %/,%,$$(dir $(1)))
$(1)_NAME := $$(notdir $(1))
$(1)_SRC  := $$(addprefix src/$$($(1)_SDIR)/,$(2))
$(1)_OBJ  := $$(addprefix obj/$$($(1)_SDIR)/,$$(patsubst %.cxx,%.o,$(2)))
$(1)_DEP  := $$(patsubst %.o,%.dep,$$($(1)_OBJ))
$(1)_LIB  := $$(addsuffix -s.a,$$(addprefix obj/lib,$$(subst /,-,$(4))))
$(1)_TDIR := priv/$$($(1)_SDIR)
$(1)_TEST := $$(addprefix $$($(1)_TDIR)/,$$($(1)_NAME))
$(1)_OUT  := $$($(1)_TEST).out
$(1)_PASS := $$($(1)_TEST).pass

$$(foreach SRC,$$($(1)_SRC),$$(eval $$(SRC)_CXXFLAGS := $(3)))

-include $$($(1)_DEP)

ifeq (,$$(findstring $$($(1)_TDIR),$(DIRS_DONE)))
DIRS_DONE += $$($(1)_TDIR)
$$($(1)_TDIR):
ifeq ($(VERBOSE),false)
	@echo ' [DIR] $$@'
endif
	$(V)mkdir -p $$@
endif

$$($(1)_OBJ): | obj/$$($(1)_SDIR)

$$($(1)_TEST): $$($(1)_OBJ) $$($(1)_LIB) | $$($(1)_TDIR)
ifeq ($(VERBOSE),false)
	@echo ' [TST] $$(@F)'
endif
	$(V)$(CXX) $(LDFLAGS) -o $$@ $$($(1)_OBJ) $$($(1)_LIB) $(5)

$$($(1)_PASS): $$($(1)_TEST)
ifeq ($(VERBOSE),false)
	@echo ' [RUN] $$(<F)'
endif
	@$$($(1)_TEST) > $$($(1)_OUT) 2>&1 && touch $$@ || (rm -f $$@ && echo "Test failed '$$($(1)_NAME)'." && cat $$($(1)_OUT))

all: $$($(1)_PASS)
endef

# $(1) path
# $(2) sources
# $(3) CXXFLAGS
# $(4) libraries (directories)
# $(5) LDFLAGS
define EXE
$(1)_SDIR := $$(patsubst %/,%,$$(dir $(1)))
$(1)_NAME := $$(notdir $(1))
$(1)_SRC  := $$(addprefix src/$$($(1)_SDIR)/,$(2))
$(1)_OBJ  := $$(addprefix obj/$$($(1)_SDIR)/,$$(patsubst %.cxx,%.o,$(2)))
$(1)_DEP  := $$(patsubst %.o,%.dep,$$($(1)_OBJ))
$(1)_LIB  := $$(addsuffix -s.a,$$(addprefix obj/lib,$$(subst /,-,$(4))))
$(1)_TDIR := dist/bin
$(1)_EXE  := $$(addprefix $$($(1)_TDIR)/,$$($(1)_NAME))

$$(foreach SRC,$$($(1)_SRC),$$(eval $$(SRC)_CXXFLAGS := $(3)))

-include $$($(1)_DEP)

ifeq (,$$(findstring $$($(1)_TDIR),$(DIRS_DONE)))
DIRS_DONE += $$($(1)_TDIR)
$$($(1)_TDIR):
ifeq ($(VERBOSE),false)
	@echo ' [DIR] $$@'
endif
	$(V)mkdir -p $$@
endif

$$($(1)_OBJ): | obj/$$($(1)_SDIR)

$$($(1)_EXE): $$($(1)_OBJ) $$($(1)_LIB) | $$($(1)_TDIR)
ifeq ($(VERBOSE),false)
	@echo ' [EXE] $$($(1)_EXE)'
endif
	$(V)$(CXX) $(LDFLAGS) -o $$@ $$($(1)_OBJ) $$($(1)_LIB) $(5)

all: $$($(1)_EXE)
endef

#
# SUPPORT
#

$(eval $(call LIBRARY,terminol/support,conv.cxx debug.cxx escape.cxx pattern.cxx time.cxx,))

$(eval $(call TEST,terminol/support/test-support,test_support.cxx,,terminol/support,))

#
# COMMON
#

$(eval $(call LIBRARY,terminol/common,ascii.cxx bit_sets.cxx buffer.cxx config.cxx data_types.cxx deduper.cxx enums.cxx key_map.cxx parser.cxx terminal.cxx tty.cxx utf8.cxx vt_state_machine.cxx,))

$(eval $(call TEST,terminol/common/test-parser,test_parser.cxx,,terminol/common terminol/support,))

$(eval $(call TEST,terminol/common/test-utf8,test_utf8.cxx,,terminol/common terminol/support,))

$(eval $(call EXE,terminol/common/abuse,abuse.cxx,,terminol/common terminol/support,))

$(eval $(call EXE,terminol/common/sequencer,sequencer.cxx,,terminol/common terminol/support,))

$(eval $(call EXE,terminol/common/styles,styles.cxx,,terminol/common terminol/support,))

$(eval $(call EXE,terminol/common/droppings,droppings.cxx,,terminol/common terminol/support,))

#
# XCB
#

$(eval $(call LIBRARY,terminol/xcb,basics.cxx color_set.cxx font_manager.cxx font_set.cxx window.cxx,$(PKG_CFLAGS)))

$(eval $(call EXE,terminol/xcb/terminol,terminol.cxx,$(PKG_CFLAGS),terminol/xcb terminol/common terminol/support,$(PKG_LDFLAGS) -lutil))

$(eval $(call EXE,terminol/xcb/terminols,terminols.cxx,$(PKG_CFLAGS),terminol/xcb terminol/common terminol/support,$(PKG_LDFLAGS) -lutil))

$(eval $(call EXE,terminol/xcb/terminolc,terminolc.cxx,,terminol/common terminol/support,))
