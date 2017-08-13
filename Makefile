INSTALL_DIR = $(HOME)/.lv2/
BUILD_DIR = build

SOURCES = $(wildcard plugins/*)
BUILD_DIRS = $(addsuffix .lv2, $(addprefix $(BUILD_DIR)/pdlv2-, $(notdir $(SOURCES))))
HEADERS = $(addsuffix /plugin.h, $(BUILD_DIRS))
PLUGINS = $(addsuffix /pdlv2.so, $(BUILD_DIRS))

LVTKLIB = lvtk/build/src/liblvtk_plugin2.a
LDFLAGS += -ldl ${LVTKLIB}
CXXFLAGS += -Wl,--no-as-needed -Wno-narrowing -shared -fPIC -DPIC -Isrc/ -std=c++11 -Ilvtk/ -Ilibpd/pure-data/src/ -Ilibpd/libpd_wrapper/
#CXXFLAGS += -Wl,--no-as-needed -Wno-narrowing -shared -fPIC -DPIC -Isrc/ -std=c++11 -Ilvtk/ -Ilibpd/pure-data/src/ -Ilibpd/libpd_wrapper/ -DPDINSTANCE 

#LIBPD_FLAGS = UTIL=true EXTRA=true ADDITIONAL_CFLAGS=-DPDINSTANCE
#LIBPD_FLAGS = ADDITIONAL_CFLAGS=-DPDINSTANCE
LIBPD_SO = libpd/libs/libpd.so

RUBY = ruby

#make the headers stick around so we can inspect them
#delete this line if you don't want them in your output directories
.SECONDARY: $(HEADERS)

all: $(PLUGINS)

$(BUILD_DIR)/pdlv2-%.lv2/pdlv2.so: $(BUILD_DIR)/pdlv2-%.lv2/plugin.h src/plugin.cpp $(LVTKLIB) $(LIBPD_SO)
	$(CXX) $(CXXFLAGS) src/plugin.cpp -I$(dir $<) -o $@ $(LIBPD_SO) $(LDFLAGS)

$(BUILD_DIR)/pdlv2-%.lv2/plugin.h: plugins/%/plugin.pd src/process.rb src/host.pd $(LIBPD_SO)
	$(RUBY) src/process.rb $< $(dir $@)
	cp $(dir $<)plugin.pd $(dir $@)

$(LVTKLIB):
	cd lvtk/ && ./waf configure && ./waf

$(LIBPD_SO):
	cd libpd/ && make libpd $(LIBPD_FLAGS)

install: $(PLUGINS)
	mkdir -p $(INSTALL_DIR)
	cp -r $(BUILD_DIR)/* $(INSTALL_DIR)

test: install
	jalv.gtk http://x37v.info/pdlv2/templateplugin.html
	#jalv.gtk http://x37v.info/pdlv2/djfilter.html
	#jalv.gtk http://x37v.info/pdlv2/mididemoplugin.html

clean:
	rm -rf build
	cd libpd/ && make clobber
