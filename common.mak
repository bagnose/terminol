# Copyright © 2013-2015 David Bryant

INSTALLDIR      ?= ~/local/terminol
VERBOSE         ?= false
VERSION         ?= $(shell git --git-dir=src/.git log -1 --format='%cd.%h' --date=short | tr -d -)
BROWSER         ?= chromium

SUPPORT_MODULES := libpcre
COMMON_MODULES  := xkbcommon
GFX_MODULES     := pangocairo pango cairo
XCB_MODULES     := cairo-xcb xcb-keysyms xcb-icccm xcb-ewmh xcb-util

ALL_MODULES     := $(SUPPORT_MODULES) $(COMMON_MODULES) $(GFX_MODULES) $(XCB_MODULES)

ifeq ($(shell pkg-config $(ALL_MODULES) && echo installed),)
  $(error Missing packages from: $(ALL_MODULES))
endif

SUPPORT_CFLAGS  := $(shell pkg-config --cflags $(SUPPORT_MODULES))
SUPPORT_LDFLAGS := $(shell pkg-config --libs   $(SUPPORT_MODULES))

COMMON_CFLAGS   := $(shell pkg-config --cflags $(SUPPORT_MODULES) $(COMMON_MODULES))
COMMON_LDFLAGS  := $(shell pkg-config --libs   $(SUPPORT_MODULES) $(COMMON_MODULES))

XCB_CFLAGS      := $(shell pkg-config --cflags $(SUPPORT_MODULES) $(COMMON_MODULES) $(GFX_MODULES) $(XCB_MODULES))
XCB_LDFLAGS     := $(shell pkg-config --libs   $(SUPPORT_MODULES) $(COMMON_MODULES) $(GFX_MODULES) $(XCB_MODULES))

CPPFLAGS        := -DVERSION=\"$(VERSION)\" -iquotesrc
CXXFLAGS        := -fpic -fno-rtti -pedantic -std=c++11 -pthread
WFLAGS          := -Wextra -Wall -Wno-long-long -Wundef                   \
                   -Wredundant-decls -Wshadow -Wsign-compare              \
                   -Wmissing-field-initializers -Wno-format-zero-length   \
                   -Wno-unused-function -Woverloaded-virtual -Wsign-promo \
                   -Wctor-dtor-privacy -Wnon-virtual-dtor
AR              := ar
ARFLAGS         := csr
LDFLAGS         := -pthread

ifneq ($(WARN),noerror)
  CXXFLAGS += -Werror
endif

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
  CXXFLAGS += -O1
else ifeq ($(MODE),coverage)
  CPPFLAGS += -DDEBUG=1
  CXXFLAGS += -g --coverage
  LDFLAGS  += --coverage
  ifeq ($(COMPILER),gnu)
    CXXFLAGS += -O1
  else
    $(error Coverage is only support by gnu compiler)
  endif
else ifeq ($(MODE),analysis)
  ifeq ($(COMPILER),clang)
    CXXFLAGS += --analyze
  else
    $(error Analysis is only support by clang compiler)
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
	@echo 'CXX     : $(CXX)'
	@echo 'CPPFLAGS: $(CPPFLAGS)'
	@echo 'CXXFLAGS: $(CXXFLAGS)'
	@echo 'LDFLAGS:  $(LDFLAGS)'

install: all

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
	@echo ' [CXX] $@'
endif
	$(V)$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WFLAGS) -c $< -o $@ -MMD -MP -MF $(patsubst %.o,%.dep,$@) $($(<)_CXXFLAGS)

# List of directories that we defined a rule for making (to prevent redefinitions).
MKDIR_DIRS :=

# Create a static library.
# $(1) directory
# $(2) sources
# $(3) CXXFLAGS
# $(4) static library (non-transitive) dependencies (directories)
define LIB
$(1)_SRC  := $$(addprefix src/$(1)/,$(2))
$(1)_OBJ  := $$(addprefix obj/$(1)/,$$(patsubst %.cxx,%.o,$(2)))
$(1)_LIB  := $$(addprefix obj/,lib$$(subst /,-,$(1)).a)
$(1)_DEP  :=

$$(foreach LIB,$(4),$$(eval $(1)_DEP += $$(LIB) $$($$(LIB)_DEP)))

$$(foreach SRC,$$($(1)_SRC),$$(eval $$(SRC)_CXXFLAGS := $(3)))

$$(foreach D,$$(patsubst %.o,%.dep,$$($(1)_OBJ)),$$(eval -include $$(D)))

ifeq (,$(findstring obj/$(2),$(MKDIR_DIRS)))
MKDIR_DIRS += obj/$(1)
obj/$(1):
ifeq ($(VERBOSE),false)
	@echo ' [DIR] $$@'
endif
	$(V)mkdir -p $$@
endif

$$($(1)_OBJ): | obj/$(1)

$$($(1)_LIB): $$($(1)_OBJ)
ifeq ($(VERBOSE),false)
	@echo ' [LIB] $$@'
endif
ifeq ($(MODE),analysis)
	$(V)touch $$@
else
	$(V)$(AR) $(ARFLAGS) $$@ $$($(1)_OBJ)
endif
endef

# Create an executable.
# $(1) type (DIST|PRIV|TEST)
# $(2) path
# $(3) sources
# $(4) CXXFLAGS
# $(5) static library (non-transitive) dependencies (directories)
# $(6) LDFLAGS
define EXE
ifeq (,$$(findstring $(1),DIST PRIV TEST))
  $$(error Bad EXE type $1)
endif

$(2)_DIR     := $$(patsubst %/,%,$$(dir $(2)))
ifeq (DIST,$(1))
$(2)_EXE_DIR := dist/bin
else
$(2)_EXE_DIR := priv/$$($(2)_DIR)
endif
$(2)_SRC_DIR := src/$$($(2)_DIR)
$(2)_OBJ_DIR := obj/$$($(2)_DIR)
$(2)_SRC     := $$(addprefix $$($(2)_SRC_DIR)/,$(3))
$(2)_OBJ     := $$(addprefix $$($(2)_OBJ_DIR)/,$$(patsubst %.cxx,%.o,$(3)))
$(2)_EXE     := $$(addprefix $$($(2)_EXE_DIR)/,$$(notdir $(2)))
$(2)_DEP     :=
$(2)_DEP_LIB :=

$$(foreach LIB,$(5),$$(eval $(2)_DEP += $$(LIB) $$($$(LIB)_DEP)))
$$(foreach LIB,$$($(2)_DEP),$$(eval $(2)_DEP_LIB += $$($$(LIB)_LIB)))

$$(foreach SRC,$$($(2)_SRC),$$(eval $$(SRC)_CXXFLAGS := $(4)))

$$(foreach D,$$(patsubst %.o,%.dep,$$($(2)_OBJ)),$$(eval -include $$(D)))

ifeq (,$$(findstring $$($(2)_OBJ_DIR),$(MKDIR_DIRS)))
MKDIR_DIRS += $$($(2)_OBJ_DIR)
$$($(2)_OBJ_DIR):
ifeq ($(VERBOSE),false)
	@echo ' [DIR] $$@'
endif
	$(V)mkdir -p $$@
endif

ifeq (,$$(findstring $$($(2)_EXE_DIR),$(MKDIR_DIRS)))
MKDIR_DIRS += $$($(2)_EXE_DIR)
$$($(2)_EXE_DIR):
ifeq ($(VERBOSE),false)
	@echo ' [DIR] $$@'
endif
	$(V)mkdir -p $$@
endif

$$($(2)_OBJ): | $$($(2)_OBJ_DIR)

$$($(2)_EXE): $$($(2)_OBJ) $$($(2)_DEP_LIB) | $$($(2)_EXE_DIR)
ifeq ($(VERBOSE),false)
	@echo ' [EXE] $$@'
endif
ifeq ($(MODE),analysis)
	$(V)touch $$@
else
	$(V)$(CXX) $(LDFLAGS) -o $$@ $$($(2)_OBJ) $(6) $$($(2)_DEP_LIB)
endif

ifeq (TEST,$(1))
$(2)_OUT  := $$($(2)_EXE).out
$(2)_PASS := $$($(2)_EXE).pass

$$($(2)_PASS): $$($(2)_EXE)
ifeq ($(VERBOSE),false)
	@echo ' [TST] $$($(2)_OUT)'
endif
ifeq ($(MODE),analysis)
	$(V)touch $$@
else
	$(V)$$($(2)_EXE) > $$($(2)_OUT) 2>&1 && touch $$@ || (rm -f $$@ && echo "Test failed '$(2)'." && cat $$($(2)_OUT)) > /dev/stderr
endif

all: $$($(2)_PASS)
else
all: $$($(2)_EXE)
endif

ifeq (DIST,$(1))
install: $(2)_INSTALL

.PHONY: $(2)_INSTALL

$(2)_INSTALL: all
	install -D $$($(2)_EXE) $(INSTALLDIR)/bin/$$(notdir $(2))
endif
endef

#
# SUPPORT
#

$(eval $(call LIB,terminol/support,conv.cxx debug.cxx pattern.cxx sys.cxx test.cxx time.cxx,$(SUPPORT_CFLAGS),))

$(eval $(call EXE,TEST,terminol/support/test-support,test_support.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-net,test_net.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-cmdline,test_cmdline.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-regex,test_regex.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-stream,test_stream.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-rle,test_rle.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-cache,test_cache.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-queue,test_queue.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-destroyer,test_destroyer.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

#
# COMMON
#

$(eval $(call LIB,terminol/common,ascii.cxx bindings.cxx bit_sets.cxx buffer.cxx config.cxx data_types.cxx escape.cxx simple_deduper.cxx enums.cxx key_map.cxx parser.cxx terminal.cxx tty.cxx utf8.cxx vt_state_machine.cxx,$(COMMON_CFLAGS),terminol/support))

$(eval $(call EXE,TEST,terminol/common/test-utf8,test_utf8.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/common/test-data-types,test_data_types.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/common/abuse,abuse.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/common/wedge,wedge.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/common/counter,counter.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/common/sequencer,sequencer.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/common/styles,styles.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/common/droppings,droppings.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/common/positioner,positioner.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/common/play,play.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/common/spinner,spinner.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

#
# XCB
#

$(eval $(call LIB,terminol/xcb,basics.cxx common.cxx color_set.cxx dispatcher.cxx font_manager.cxx font_set.cxx screen.cxx widget.cxx,$(XCB_CFLAGS),terminol/common))

$(eval $(call EXE,DIST,terminol/xcb/terminol,terminol.cxx,$(XCB_CFLAGS),terminol/xcb,$(XCB_LDFLAGS) -lutil))

$(eval $(call EXE,DIST,terminol/xcb/terminols,terminols.cxx,$(XCB_CFLAGS),terminol/xcb,$(XCB_LDFLAGS) -lutil))

$(eval $(call EXE,DIST,terminol/xcb/terminolc,terminolc.cxx,$(XKB_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/xcb/widget-test,widget_test.cxx,$(XKB_CFLAGS),terminol/xcb,$(XCB_LDFLAGS)))
