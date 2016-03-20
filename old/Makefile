#  Makefile - part of Puredata (pd) to LV2 Plugin bridge.
#  Copyright 2012 Martin Schied <m@unknown-error.com>,
#   
#  Permission to use, copy, modify, and/or distribute this software for any
#  purpose with or without fee is hereby granted, provided that the above
#  copyright notice and this permission notice appear in all copies.
# 
#  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
#  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
#  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 

 # todo: 
 # test if ~.lv2 dir exists
 # make libpd a dependency
 # install libpd if not yet installed in the users's system
 # does not link if libpd.so is not in /usr/lib at the moment.
 
 
PDFILES := $(wildcard LV2*.pd)
BUNDLEDIRS := $(patsubst %.pd,%.lv2 ,$(PDFILES))
BASENAMES := $(patsubst %.pd,%,$(PDFILES))
BUNDLEDIR_PD := $(foreach basename, $(BASENAMES), $(basename).lv2/$(basename).pd)
SOFILES := $(foreach basename, $(BASENAMES), $(basename).lv2/pdLV2-stereo.so)
CFILES := $(foreach basename, $(BASENAMES), $(basename).lv2/pdLV2-stereo.c)
MANIFESTS := $(foreach basename, $(BASENAMES), $(basename).lv2/manifest.ttl)
TTLFILES := $(foreach basename, $(BASENAMES), $(basename).lv2/pdLV2-stereo.ttl)
URIFILES := $(foreach basename, $(BASENAMES), $(basename).lv2/pluginURI.txt)
CONTROLSFILES := $(foreach basename, $(BASENAMES), $(basename).lv2/controls.txt)

all: $(BUNDLEDIRS) $(PDFILES) $(TTLFILES) $(SOFILES) $(MANIFESTS)
 
# Patch the c code for the URIFILES

#replacements: $(CONTROLSFILES)

%/pdLV2-stereo.ttl: %/controls.txt %/pluginURI.txt pdLV2-stereo.ttl-template

	@echo --- Replacing controls and URI: for $(@D) ---
	./replace.sh $(@D)/controls.txt pdLV2-stereo.ttl-template $(@D)/pdLV2-stereo.ttl
	./replace.sh $(@D)/pluginURI.txt $(@D)/pdLV2-stereo.ttl $(@D)/pdLV2-stereo.ttl
	
%/pdLV2-stereo.c: %/pluginURI.txt pdLV2-stereo.c-template
	@echo --- Replacing controls and URI: for $(@D) ---
	./replace.sh $(@D)/pluginURI.txt pdLV2-stereo.c-template $(@D)/pdLV2-stereo.c
	sed -i "s/pdPatchFile/$(patsubst %.lv2,%.pd,$(@D))/g" $(@D)/pdLV2-stereo.c
	@echo done patching.
	cp $(@D)/pdLV2-stereo.c $(@D)/pdLV2-stereo.c.bak

%/manifest.ttl: %/pluginURI.txt manifest.ttl-template
	@echo --- Replacing controls and URI: for $(@D) ---
	./replace.sh $(@D)/pluginURI.txt manifest.ttl-template $(@D)/manifest.ttl

%/pdLV2-stereo.so: %/pdLV2-stereo.c
	
	@echo compiling $(@D):
	gcc -shared -fPIC -std=c99 -Ilibpd/pure-data/src/ -Ilibpd/libpd_wrapper -L/usr/lib/ -Wl,--no-as-needed -lpd -I/usr/lib/lv2 $(@D)/pdLV2-stereo.c -o $(@D)/pdLV2-stereo.so
	#gcc -shared -fPIC -std=c99 -Ilibpd/pure-data/src/ -Ilibpd/libpd_wrapper -Llibpd/libs -lpd -I/usr/lib/lv2 $(@D)/pdLV2-stereo.c -o $(@D)/pdLV2-stereo.so
	@echo done compiling


# This rule should be called: pdfilesInBundledir:
# this is very, very ugly. cannot figure out an other method so @ contains a useable name.

%.lv2: %.pd
	@echo Creating/Updating Bundle Directory $(@)
	mkdir -p $(@)
	touch $(@)
	cp $(patsubst %.lv2,%.pd,$(@)) $(@)
	./getURIs.sh $@/$(patsubst %.lv2,%.pd,$(@)) $@/pluginURI.txt
	echo"" > $@/controls.txt
	./getControls.sh $@/$(patsubst %.lv2,%.pd,$(@)) >> $@/controls.txt

clean: 
	rm -r $(BUNDLEDIRS)


install: 
	cp -R *.lv2 ~/.lv2