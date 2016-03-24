pdlv2
====

Makes simple [Pure Data (aka pd)](http://msp.ucsd.edu/software.html) patches usable as [LV2 plugins](http://lv2plug.in/) utilizing [libpd](https://github.com/libpd/libpd) and [lv2-c++-tools](http://www.nongnu.org/ll-plugins/hacking.html)

Linux only at the moment.

NOTE
----

Need to search for libpd.so and copy it into bundle instead of having it in top project dir??
Can consolidate the templates and pass arguments to function pointers with std::forward ?
http://en.cppreference.com/w/cpp/utility/forward

Try multiple intances of libpd as a shared object?:
* http://stackoverflow.com/questions/1716472/using-libtool-to-load-a-duplicate-function-name-from-a-shared-library
* http://stackoverflow.com/questions/1745975/load-multiple-copies-of-a-shared-library
* carla --gdb for testing with multiple plugins at a time.. though my version doesn't seem to invoke gdb
* jalv.. how do we run it in GDB and load our plugin's symbols?

Your lv2 host needs to provide processing blocks in multiples of pd's block size, which defaults to 64. 
Hopefully we can relax this requirement in the future.


Requirements
----

Only works on Linux as far as I know

Install these:
* [Pure Data](http://msp.ucsd.edu/software.html) can use package manager
* [libpd](https://github.com/libpd/libpd)
	* included as a submodule so just run the following then the make process should work
	* _git submodule init_
	* _git submodule update_
	* then do the same in the _libpd_ directory
* [lv2-c++-tools](http://www.nongnu.org/ll-plugins/hacking.html) can use package manager
* [ruby](https://www.ruby-lang.org) use package manager or rvm
* [bundler](http://bundler.io/) gem or package manager
  * run: **bundle install** from the top project directory

You'll also need a c++ compiler and **make**
* install **build-essential** on debian systems


Workflow
----
* create your pd patch, save it as *plugins/\<plugin_name\>/plugin.pd*
  * check out the template patch as a reference
  * specify, using messages:
    * | **pluginURI**: *http://uniqueURI/for/your.plugin* (
    * | **pluginName**: *name of your plugin* (
    * | **pluginLicense**: *http://rdf-uri/to/your/license/* (
      * defaults to 'http://usefulinc.com/doap/licenses/gpl'
  * specify control inputs with **receive** objects
    * format: [ **receive** *$1-lv2-control_symbol_name* **label:** *Control-Label* **range:** *0 0.5 1* ]
    * you can use the short form: [ **r** _$1-lv2-control_symbol_name_ ]
    * the symbol must be *$1-lv2-* followed by a valid c-identifier which defines the symbol that is used to identify the port
      * for example, *$1-lv2-balance* defines a port named *balance*
    * label cannot contain whitespace at this time
    * range specified by floats in the order: *minimum* *default* *maximum*
  * specify control outputs with **send** objects
    * the format is the same as the **receive** except it uses a **send** object
    * you can use the short form: [ **s** _$1-lv2-control_symbol_name_ ]
  * specify any audio inputs or outputs you want
    * inputs use **adc~**
      * example: 4 inputs would be [ *adc~ 1 2 3 4* ]
    * outputs use **dac~**
    * without arguments, or with *1 2*, they default to Input or Output Left, Right
  * include any abstractions in the same directory
* **make**: to build all the plugins
  * this parses the pd file for all the info it needs to build the plugins
* **make install**: to install into ~/.lv2/
  * you can modify this destination by editing *INSTALL_DIR* in the Makefile


References
----

* [pdLV2-stereo](https://github.com/unknownError/pdLV2-stereo)
  * a libpd-lv2 plugin wrapper by [Martin Schied](https://github.com/unknownError) that [xnor](http://x37v.info) worked from to become the **pdlv2**
  * some of the example plugins come from the **pdLV2-stereo** project, slightly altered to work with this project
* Lars Luthman's [LV2 programming for the complete idiot](http://www.nongnu.org/ll-plugins/lv2pftci/)
  * the example [xnor](http://x37v.info) referenced to create the c++ plugin wrapper
* [RDF for Ruby](http://blog.datagraph.org/2010/03/rdf-for-ruby)
  * the library used to generate the rdf plugin specification
* [doap rdf](https://github.com/edumbill/doap/) Documentation of a project
* [LV2 plugins](http://lv2plug.in/)


TODO
----

* allow specification of audio input and output names
* allow for block sizes less than 64
* allow whitespace in control labels
* [plugin category](http://www.nongnu.org/ll-plugins/lv2pftci/#More_metadata)
* [port groups](http://www.nongnu.org/ll-plugins/lv2pftci/#Port_groups)
* [midi](http://lv2plug.in/ns/ext/midi#MidiEvent)
