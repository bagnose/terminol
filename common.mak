# Copyright © 2013-2017 David Bryant

INSTALLDIR      ?= ~/local/terminol
VERBOSE         ?= false
VERSION         ?= $(shell git --git-dir=src/.git log -1 --format='%cd.%h' --date=short | tr -d -)
BROWSER         ?= chromium
CLANG-C++       ?= clang++
CLANG-AR        ?= llvm-ar
GNU-C++         ?= g++
GNU-AR          ?= ar

SUPPORT_MODULES := libpcre
COMMON_MODULES  := xkbcommon
GFX_MODULES     := pangocairo pango cairo
XCB_MODULES     := cairo-xcb xcb-keysyms xcb-icccm xcb-ewmh xcb-util

ALL_MODULES     := $(SUPPORT_MODULES) $(COMMON_MODULES) $(GFX_MODULES) $(XCB_MODULES)

ifeq ($(shell pkg-config $(ALL_MODULES) && echo installed),)
  $(error $(shell pkg-config --print-errors $(ALL_MODULES)))
endif

SUPPORT_CFLAGS  := $(shell pkg-config --cflags $(SUPPORT_MODULES) | sed 's|-I/|-isystem /|')
SUPPORT_LDFLAGS := $(shell pkg-config --libs   $(SUPPORT_MODULES))

COMMON_CFLAGS   := $(shell pkg-config --cflags $(SUPPORT_MODULES) $(COMMON_MODULES) | sed 's|-I/|-isystem /|')
COMMON_LDFLAGS  := $(shell pkg-config --libs   $(SUPPORT_MODULES) $(COMMON_MODULES))

XCB_CFLAGS      := $(shell pkg-config --cflags $(SUPPORT_MODULES) $(COMMON_MODULES) $(GFX_MODULES) $(XCB_MODULES) | sed 's|-I/|-isystem /|')
XCB_LDFLAGS     := $(shell pkg-config --libs   $(SUPPORT_MODULES) $(COMMON_MODULES) $(GFX_MODULES) $(XCB_MODULES))

CPPFLAGS        := -DVERSION=\"$(VERSION)\" -iquote src
CXXFLAGS        := -fpic -fno-rtti -std=c++17 -pthread
WFLAGS          := -Wpedantic -Wextra -Wall -Wundef -Wshadow              \
                   -Wredundant-decls -Wsign-compare                       \
                   -Wmissing-declarations -Wold-style-cast                \
                   -Wmissing-field-initializers -Wno-format-zero-length   \
                   -Wno-unused-function -Woverloaded-virtual -Wsign-promo \
                   -Wctor-dtor-privacy -Wnon-virtual-dtor
ARFLAGS         := csr
LDFLAGS         := -pthread

ifneq ($(WARN),noerror)
  CXXFLAGS += -Werror
endif

ifeq ($(COMPILER),gnu)
  WFLAGS += -Wsuggest-override -Wno-virtual-move-assign
  # XXX next line not to be merged with master:
  WFLAGS += -Wno-error=unused-parameter
  CXX := $(GNU-C++)
  AR  := $(GNU-AR)
else ifeq ($(COMPILER),clang)
  WFLAGS += -Wextra-semi -Wcomma
  CXXFLAGS += -stdlib=libc++
  LDFLAGS += -stdlib=libc++
  # XXX next line not to be merged with master:
  #WFLAGS += -Wno-error=unused-parameter
  WFLAGS += -Wno-unused-parameter
  CXX := $(CLANG-C++)
  AR  := $(CLANG-AR)
else
  $(error Unrecognised COMPILER: $(COMPILER))
endif

ifeq ($(MODE),release)
  CPPFLAGS += -DDEBUG=0 -DNDEBUG
  WFLAGS   += -Wno-unused-parameter
  ifeq ($(COMPILER),gnu)
    CXXFLAGS += -Os
    CXXFLAGS += -Wno-unused-but-set-variable
  else
    CXXFLAGS += -Oz
  endif
else ifeq ($(MODE),debug)
  CPPFLAGS += -DDEBUG=1
  CXXFLAGS += -g
  #CXXFLAGS += -O1
  # XXX not to be merged with master:
  CXXFLAGS += -O0
else ifeq ($(MODE),coverage)
  CPPFLAGS += -DDEBUG=1
  CXXFLAGS += -g --coverage
  LDFLAGS  += --coverage
  ifeq ($(COMPILER),gnu)
    CXXFLAGS += -O1
  else
    $(error Coverage is only supported by gnu compiler)
  endif
else ifeq ($(MODE),analysis)
  ifeq ($(COMPILER),clang)
    CXXFLAGS += --analyze
  else
    $(error Analysis is only supported by clang compiler)
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
	$(V)$(CXX) -c $< -o $@ $(CPPFLAGS) $(CXXFLAGS) $(WFLAGS) $($(<)_CXXFLAGS) -MMD -MP -MF $(patsubst %.o,%.dep,$@)
ifeq ($(COMPILER),clang)
	$(V)rc --compile $(CXX) -c $< -o $@ $(CPPFLAGS) $(CXXFLAGS) $(WFLAGS) $($(<)_CXXFLAGS)
endif

# List of directories that we defined a rule for making (to prevent redefinitions)
MKDIR_DIRS :=

# Create a static library
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

# Create an executable
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
	$(V)$(CXX) $(LDFLAGS) -o $$@ $$($(2)_OBJ) $$($(2)_DEP_LIB) $(6)
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

$(eval $(call LIB,terminol/support,exception.cxx conv.cxx debug.cxx pattern.cxx pipe.cxx sys.cxx test.cxx time.cxx,$(SUPPORT_CFLAGS),))

$(eval $(call EXE,TEST,terminol/support/test-pattern,test_pattern.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-support,test_support.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-net,test_net.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-cmdline,test_cmdline.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-regex,test_regex.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-stream,test_stream.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-rle,test_rle.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-cache,test_cache.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-math,test_math.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/support/test-queue,test_queue.cxx,$(SUPPORT_CFLAGS),terminol/support,$(SUPPORT_LDFLAGS)))

#
# COMMON
#

$(eval $(call LIB,terminol/common,ascii.cxx bindings.cxx bit_sets.cxx buffer.cxx config.cxx data_types.cxx dedupe_repository.cxx enums.cxx escape.cxx key_map.cxx para.cxx para_cache.cxx parser.cxx simple_deduper.cxx simple_repository.cxx terminal.cxx text.cxx tty.cxx utf8.cxx vt_state_machine.cxx,$(COMMON_CFLAGS),terminol/support))

$(eval $(call EXE,TEST,terminol/common/test-utf8,test_utf8.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/common/test-data-types,test_data_types.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,PRIV,terminol/common/abuse,abuse.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/common/test-para,test_para.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/common/test-text,test_text.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

$(eval $(call EXE,TEST,terminol/common/test-buffer2,test_buffer2.cxx,$(COMMON_CFLAGS),terminol/common,$(COMMON_LDFLAGS)))

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
