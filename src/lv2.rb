require 'rdf'
require 'linkeddata'


class LV2
  attr_accessor :lv2_include_dir
  def initialize
    [ENV['LV2_INCLUDE_DIR'], '/usr/local/include/lv2', '/usr/include/lv2'].each do |dir|
      next unless dir
      p = File.join(dir, "lv2plug.in/")
      if File.directory?(p)
        @lv2_include_dir = p
        break
      end
    end
  end

  def plugin_classes
    sc = RDF::URI.new("http://www.w3.org/2000/01/rdf-schema#subClassOf")
    f = File.join(@lv2_include_dir, "/ns/lv2core/lv2core.ttl")
    g = RDF::Graph.load(f)
    types = [ RDF::URI.new("http://lv2plug.in/ns/lv2core#Plugin") ]
    types.each do |t|
      g.query([nil, sc, t]).each do |s|
        types << s[0]
      end
    end
    return types.uniq
  end

  def port_groups
    sc = RDF::URI.new("http://www.w3.org/2000/01/rdf-schema#subClassOf")
    f = File.join(@lv2_include_dir, "/ns/ext/port-groups/port-groups.ttl")
    g = RDF::Graph.load(f)
    types = [ RDF::URI.new("http://lv2plug.in/ns/ext/port-groups#Group") ]
    types.each do |t|
      g.query([nil, sc, t]).each do |s|
        types << s[0]
      end
    end
    return types.uniq
  end
end

x = LV2.new
puts x.lv2_include_dir
puts x.port_groups.join("\n")
puts x.plugin_classes.join("\n")
