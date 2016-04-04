pdlv2
====

Makes simple [Pure Data (aka pd)](http://msp.ucsd.edu/software.html) patches usable as [LV2 plugins](http://lv2plug.in/) utilizing [libpd](https://github.com/libpd/libpd) and [lv2-c++-tools](http://www.nongnu.org/ll-plugins/hacking.html)

It supports audio, control and midi in and out.

Linux only at the moment.

NOTE
----

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
* install **build-essential** on debian based systems


Workflow
----

### Short version:

* Install all the stuff in the **requirements** above
* Copy the entire **plugins/template/** directory to a new directory in **plugins/**
  * For example *plugins/myawesomeplugin*
* Use *Pure Data* to customize the **plugin.pd** file in that new directory
  * In the above example run: *pd plugins/myawesomeplugin/plugin.pd*
* Run **make install** from the top level of the project
* **done**

### Long version:

* create your pd patch, save it as *plugins/\<plugin_name\>/plugin.pd*
  * check out the template patch as a reference
  * specify the plugin details using messages:
    * | **pluginURI**: *http://uniqueURI/for/your.plugin* (
    * | **pluginName**: *name of your plugin* (
    * | **pluginLicense**: *http://rdf-uri/to/your/license* (
      * defaults to 'http://usefulinc.com/doap/licenses/gpl'
    * | **pluginMaintainer**: *Your Name* (
  * specify control inputs and/or outputs you want
    * inputs use **[receive]** or **[r]**
    * outputs use **[send]** or **[s]**
    * format: [ **receive** *$1-lv2-control_symbol_name* **label:** *Control-Label* **range:** *0 0.5 1* ]
    * you can use the short form: [ **r** _$1-lv2-control_symbol_name_ ]
    * the symbol must be *$1-lv2-* followed by a valid c-identifier which defines the symbol that is used to identify the port
      * for example, *$1-lv2-balance* defines a port named *balance*
    * label cannot contain whitespace at this time
    * range specified by floats in the order: *minimum* *default* *maximum*
  * specify any audio inputs and/or outputs you want
    * inputs use **[inlet~]**
    * outputs use **[outlet~]**
      * name them with a label or a group
        * label:Label-Name
        * group:GroupName:GroupType:MemberType
      * you only need a label or a group, not both
      * _GroupType_ and _MemberType_ are defined by lv2
      * for example [inlet~ label:Ducking-Input]
      * for example [outlet~ group:Main:StereoGroup:right]
  * include midi objects if you want a midi in or out ports, can be in a subpatch but not an abstraction
    * for input:
      * notein
      * ctlin
      * pgmin
      * bendin
      * touchin
      * polytouchin
    * for output:
      * noteout
      * ctlout
      * pgmout
      * bendout
      * touchout
      * polytouchout
  * include any abstractions in the same directory
* **make**: to build all the plugins
  * this parses the pd file for all the info it needs to build the plugins
* **make install**: to install into ~/.lv2/
  * you can modify this destination by editing *INSTALL_DIR* in the Makefile


References
----

* [lv2 categories](http://lv2plug.in/ns/lv2core/)
* [lv2 Specifications](http://lv2plug.in/ns/)
* [pdLV2-stereo](https://github.com/unknownError/pdLV2-stereo)
  * a libpd-lv2 plugin wrapper by [Martin Schied](https://github.com/unknownError) that [xnor](http://x37v.info) worked from to become the **pdlv2**
  * some of the example plugins come from the **pdLV2-stereo** project, slightly altered to work with this project
* Lars Luthman's [LV2 programming for the complete idiot](http://www.nongnu.org/ll-plugins/lv2pftci/)
  * the example [xnor](http://x37v.info) referenced to create the c++ plugin wrapper
* [RDF for Ruby](http://blog.datagraph.org/2010/03/rdf-for-ruby)
  * the library used to generate the rdf plugin specification
* [doap rdf](https://github.com/edumbill/doap/) Documentation of a project
* [LV2 plugins](http://lv2plug.in/)


Debugging, dev
----

* carla --gdb for testing with multiple plugins at a time.. though my version doesn't seem to invoke gdb
* jalv.. how do we run it in GDB and load our plugin's symbols?


TODO
----

* support externals
* ditch temp file plugin loading when libpd has real multi-instance support
* consolidate libpd calls into a class and preload function pointers
* [validate](http://lv2plug.in/pages/validating-lv2-data.html)
* allow for block sizes less than 64
* allow whitespace in control labels
* [plugin category](http://www.nongnu.org/ll-plugins/lv2pftci/#More_metadata)
  * [reference](http://lv2plug.in/ns/lv2core/#sec-reference)
* support a custom GUI
