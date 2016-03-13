INSTALL_DIR = $(HOME)/.lv2/
NAME = xnor
BUNDLE = lv2x37v-$(NAME).lv2

CXXFLAGS = -Wl,--no-as-needed -lpd -shared -fPIC -DPIC plugin.cpp -std=c++11 `pkg-config --cflags --libs lv2-plugin`

$(BUNDLE): manifest.ttl details.ttl patch.pd $(NAME).so
	rm -rf $(BUNDLE)
	mkdir $(BUNDLE)
	cp manifest.ttl details.ttl *.pd $(NAME).so $(BUNDLE)

$(NAME).so: plugin.cpp
	g++ $(CXXFLAGS) -o $(NAME).so

install: $(BUNDLE)
	mkdir -p $(INSTALL_DIR)
	rm -rf $(INSTALL_DIR)/$(BUNDLE)
	cp -R $(BUNDLE) $(INSTALL_DIR)

clean:
	rm -rf $(BUNDLE) *.so

test: install
	jalv -p http://xnor.info/lv2/stereopanner
