INSTALL_DIR = $(HOME)/.lv2/
BUILD_DIR = build

SOURCES = $(wildcard plugins/*)
BUILD_DIRS = $(addprefix $(BUILD_DIR)/, $(notdir $(SOURCES)))
HEADERS = $(addsuffix /plugin.h, $(BUILD_DIRS))
PLUGINS = $(addsuffix /pdlv2.so, $(BUILD_DIRS))

CXXFLAGS = -Wl,--no-as-needed -lpd -shared -fPIC -DPIC -I. -std=c++11 `pkg-config --cflags --libs lv2-plugin`

$(BUILD_DIR)/%/pdlv2.so: $(BUILD_DIR)/%/plugin.h
	g++ $(CXXFLAGS) plugin.cpp -I$(dir $<) -o $@

$(BUILD_DIR)/%/plugin.h: plugins/%/plugin.pd
	ruby parse.rb $< $(dir $@)

all: $(PLUGINS)

clean:
	rm -rf build
