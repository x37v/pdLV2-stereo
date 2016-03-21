INSTALL_DIR = $(HOME)/.lv2/
BUILD_DIR = build

SOURCES = $(wildcard plugins/*)
BUILD_DIRS = $(addsuffix .lv2, $(addprefix $(BUILD_DIR)/pdlv2-, $(notdir $(SOURCES))))
HEADERS = $(addsuffix /plugin.h, $(BUILD_DIRS))
PLUGINS = $(addsuffix /pdlv2.so, $(BUILD_DIRS))

CXXFLAGS = -Wl,--no-as-needed -lpd -shared -fPIC -DPIC -I. -std=c++11 `pkg-config --cflags --libs lv2-plugin`

all: $(PLUGINS)

$(BUILD_DIR)/pdlv2-%.lv2/pdlv2.so: $(BUILD_DIR)/pdlv2-%.lv2/plugin.h
	g++ $(CXXFLAGS) plugin.cpp -I$(dir $<) -o $@

$(BUILD_DIR)/pdlv2-%.lv2/plugin.h: plugins/%/plugin.pd
	ruby parse.rb $< $(dir $@)

install: $(PLUGINS)
	cp -r $(BUILD_DIR)/* $(INSTALL_DIR)

clean:
	rm -rf build
