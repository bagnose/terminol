VERBOSE    ?= false
MODE       ?= release
BUILDDIR   ?= build
INSTALLDIR ?= install
CCTYPE     ?= gnu

PCMODULES   = pangocairo pango cairo fontconfig xcb-keysyms xcb-icccm xcb-ewmh xcb-util xkbcommon

ifeq ($(shell pkg-config $(PCMODULES) && echo installed),)
  $(error Some package is not installed from: $(PCMODULES))
endif

PKG_CFLAGS  = $(shell pkg-config --cflags $(PCMODULES))
PKG_LDFLAGS = $(shell pkg-config --libs   $(PCMODULES))

CXXFLAGS    = -MD -iquote. -fpic -pedantic -Werror -Wextra -Wall -Wno-long-long -Wundef -Wredundant-decls -Wshadow -Wsign-compare -Wredundant-decls -Wmissing-field-initializers -Wno-format-zero-length -Wno-unused-function -DVERSION=\"0.0\" -std=c++11 -Woverloaded-virtual -Wsign-promo -Wctor-dtor-privacy -Wnon-virtual-dtor -fno-rtti $(PKG_CFLAGS)
AR          = ar
ARFLAGS     = csr
LDFLAGS     =

ifeq ($(CCTYPE),gnu)
  CXX = g++
else ifeq ($(CCTYPE),clang)
  CXX = clang++
else
  $(error Unrecognised CCTYPE: $(MODE))
endif

ifeq ($(MODE),release)
  CXXFLAGS += -DDEBUG=0 -DNDEBUG -Wno-unused-parameter
  ifeq ($(CCTYPE),gnu)
    CXXFLAGS += -Os
  else
    CXXFLAGS += -O4
    LDFLAGS  += -O4
  endif
else ifeq ($(MODE),debug)
  CXXFLAGS += -ggdb3 -DDEBUG=1
  ifeq ($(CCTYPE),gnu)
    CXXFLAGS += -Og
  else
    CXXFLAGS += -O1
  endif
else ifeq ($(MODE),coverage)
  CXXFLAGS += $(OSLOW) -ggdb3 -DDEBUG=1 --coverage
  LDFLAGS  += --coverage
  ifeq ($(CCTYPE),gnu)
    CXXFLAGS += -Og
  else
    $(error Coverage is only support by gnu compiler)
  endif
else
  $(error Unrecognised MODE: $(MODE))
endif

ifeq ($(VERBOSE),false)
  V=@
else ifeq ($(VERBOSE),true)
  V=
else
  $(error Unrecognised VERBOSE: $(VERBOSE))
endif

all:

.PHONY: clean

install: all
	install -D $(BUILDDIR)/dist/bin/terminol $(INSTALLDIR)/bin/terminol
	install -D $(BUILDDIR)/dist/bin/terminols $(INSTALLDIR)/bin/terminols
	install -D $(BUILDDIR)/dist/bin/terminolc $(INSTALLDIR)/bin/terminolc

clean:
	rm -rf $(BUILDDIR)/obj $(BUILDDIR)/dist

aur:
	cd pkg && rm -f *.xz && makepkg -s && rm -rf pkg src terminol

$(BUILDDIR)/obj/%.o: %.cxx
ifeq ($(VERBOSE),false)
	@echo " [CXX] $(<F)"
endif
	$(V)$(CXX) $(CXXFLAGS) -c $< -o $@

# $(1) stem
# $(2) name
# $(3) directory
# $(4) sources
define LIBRARY
$(1)_SRC = $$(addprefix $(3)/,$(4))
$(1)_OBJ = $$(addprefix $(BUILDDIR)/obj/,$$(patsubst %.cxx,%.o,$$($(1)_SRC)))
$(1)_DEP = $$(patsubst %.o,%.d,$$($(1)_OBJ))
$(1)_LIB = $$(addprefix $(BUILDDIR)/obj/,lib$(2)-s.a)

-include $$($(1)_DEP)

ifeq (,$(findstring $(BUILDDIR)/obj/$(3),$(DIRS_DONE)))
DIRS_DONE += $(BUILDDIR)/obj/$(3)
$(BUILDDIR)/obj/$(3):
ifeq ($(VERBOSE),false)
	@echo " [DIR] $$@"
endif
	$(V)mkdir -p $$@
endif

$$($(1)_OBJ): | $(BUILDDIR)/obj/$(3)

$$($(1)_LIB): $$($(1)_OBJ)
ifeq ($(VERBOSE),false)
	@echo " [LIB] $$(@F)"
endif
	$(V)$(AR) $(ARFLAGS) $$@ $$($(1)_OBJ)
endef

# $(1) stem
# $(2) name
# $(3) directory
# $(4) sources
# $(5) libraries
# $(6) LDFLAGS
define EXE
$(1)_SRC = $$(addprefix $(3)/,$(4))
$(1)_OBJ = $$(addprefix $(BUILDDIR)/obj/,$$(patsubst %.cxx,%.o,$$($(1)_SRC)))
$(1)_DEP = $$(patsubst %.o,%.d,$$($(1)_OBJ))
$(1)_EXE = $$(addprefix $(BUILDDIR)/dist/bin/,$(2))
$(1)_LIB = $$(addsuffix -s.a,$$(addprefix $(BUILDDIR)/obj/lib,$(5)))

-include $$($(1)_DEP)

ifeq (,$(findstring $(BUILDDIR)/dist/bin,$(DIRS_DONE)))
DIRS_DONE += $(BUILDDIR)/dist/bin
$(BUILDDIR)/dist/bin:
ifeq ($(VERBOSE),false)
	@echo " [DIR] $$@"
endif
	$(V)mkdir -p $$@
endif

$$($(1)_OBJ): | $(BUILDDIR)/obj/$(3)

$$($(1)_EXE): $$($(1)_OBJ) $$($(1)_LIB) | $(BUILDDIR)/dist/bin
ifeq ($(VERBOSE),false)
	@echo " [EXE] $$($(1)_EXE)"
endif
	$(V)$(CXX) $(LDFLAGS) -o $$@ $$($(1)_OBJ) $$($(1)_LIB) $(6)

all: $$($(1)_EXE)
endef

$(eval $(call LIBRARY,SUPPORT,terminol-support,terminol/support,conv.cxx debug.cxx escape.cxx pattern.cxx time.cxx))

$(eval $(call LIBRARY,COMMON,terminol-common,terminol/common,abuse.cxx ascii.cxx bit_sets.cxx buffer.cxx config.cxx data_types.cxx deduper.cxx droppings.cxx enums.cxx key_map.cxx parser.cxx sequencer.cxx styles.cxx terminal.cxx test_parser.cxx test_support.cxx test_utf8.cxx tty.cxx utf8.cxx vt_state_machine.cxx))

$(eval $(call LIBRARY,XCB,terminol-xcb,terminol/xcb,basics.cxx color_set.cxx font_manager.cxx font_set.cxx window.cxx))

$(eval $(call EXE,TERMINOL,terminol,terminol/xcb,terminol.cxx,terminol-xcb terminol-common terminol-support,$(PKG_LDFLAGS) -lutil))

$(eval $(call EXE,TERMINOLC,terminolc,terminol/xcb,terminolc.cxx,terminol-xcb terminol-common terminol-support,$(PKG_LDFLAGS) -lutil))

$(eval $(call EXE,TERMINOLS,terminols,terminol/xcb,terminols.cxx,terminol-xcb terminol-common terminol-support,$(PKG_LDFLAGS) -lutil))
