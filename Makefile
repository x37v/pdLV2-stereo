INSTALL_DIR = $(HOME)/.lv2/
NAME = template
BUNDLE = $(NAME).lv2
BUILD_DIR = build/$(BUNDLE)

CXXFLAGS = -Wl,--no-as-needed -lpd -shared -fPIC -DPIC -I. -std=c++11 `pkg-config --cflags --libs lv2-plugin`

$(BUILD_DIR): $(BUILD_DIR)/manifest.ttl $(BUILD_DIR)/details.ttl $(BUILD_DIR/plugin.pd $(BUILD_DIR)/pdlv2.so
	rm -rf $(BUNDLE)
	mkdir $(BUNDLE)
	cp plugins/$(NAME)/*.pd $(BUNDLE)

$(BUILD_DIR)/pdlv2.so: plugin.cpp
	g++ $(CXXFLAGS) plugin.cpp -Ibuild/$(BUNDLE)/ -o $(BUILD_DIR)/pdlv2.so

install: $(BUILD_DIR)
	mkdir -p $(INSTALL_DIR)
	rm -rf $(INSTALL_DIR)/$(BUNDLE)
	cp -R build/$(BUNDLE) $(INSTALL_DIR)

clean:
	rm -rf $(BUNDLE) *.so

test: install
	jalv -p http://xnor.info/lv2/stereopanner
