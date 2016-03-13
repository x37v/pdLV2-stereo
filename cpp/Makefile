INSTALL_DIR = $(HOME)/.lv2/
NAME = xnor
BUNDLE = lv2x37v-$(NAME).lv2

$(BUNDLE): manifest.ttl $(NAME).so
	rm -rf $(BUNDLE)
	mkdir $(BUNDLE)
	cp manifest.ttl $(NAME).so $(BUNDLE)

$(NAME).so: plugin.cpp
	g++ -shared -fPIC -DPIC plugin.cpp -std=c++11 `pkg-config --cflags --libs lv2-plugin` -o $(NAME).so

install: $(BUNDLE)
	mkdir -p $(INSTALL_DIR)
	rm -rf $(INSTALL_DIR)/$(BUNDLE)
	cp -R $(BUNDLE) $(INSTALL_DIR)

clean:
	rm -rf $(BUNDLE) *.so
