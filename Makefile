INSTALL_DIR = $(HOME)/.lv2/
BUILD_DIR = build

SOURCES = $(wildcard plugins/*)
BUILD_DIRS = $(addsuffix .lv2, $(addprefix $(BUILD_DIR)/pdlv2-, $(notdir $(SOURCES))))
HEADERS = $(addsuffix /plugin.h, $(BUILD_DIRS))
PLUGINS = $(addsuffix /pdlv2.so, $(BUILD_DIRS))

LDFLAGS = -lpd -L/usr/local/lib `pkg-config --libs lv2-plugin`
CXXFLAGS = -Wall -shared -fPIC -DPIC -I. -std=c++11 `pkg-config --cflags lv2-plugin`

#make the headers stick around so we can inspect them
#delete this line if you don't want them in your output directories
.SECONDARY: $(HEADERS)

all: $(PLUGINS)

$(BUILD_DIR)/pdlv2-%.lv2/pdlv2.so: $(BUILD_DIR)/pdlv2-%.lv2/plugin.h
	g++ $(CXXFLAGS) plugin.cpp -I$(dir $<) -o $@ $(LDFLAGS)

$(BUILD_DIR)/pdlv2-%.lv2/plugin.h: plugins/%/plugin.pd
	ruby parse.rb $< $(dir $@)
	cp $(dir $<)/* $(dir $@)

install: $(PLUGINS)
	mkdir -p $(INSTALL_DIR)
	cp -r $(BUILD_DIR)/* $(INSTALL_DIR)

clean:
	rm -rf build
