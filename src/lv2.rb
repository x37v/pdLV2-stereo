#!/usr/bin/env ruby
#
require 'rdf'
require 'linkeddata'
require 'rdf/ntriples'

class LV2
  attr_accessor :lv2_include_dir
  attr_reader :port_groups_uri
  attr_reader :port_groups
  attr_reader :plugin_classes

  def initialize
    find_lv2_dir

    @rdf_subclass = RDF::URI.new("http://www.w3.org/2000/01/rdf-schema#subClassOf")

    @port_groups_uri = RDF::URI.new("http://lv2plug.in/ns/ext/port-groups#")

    @lv2  = RDF::Vocabulary.new("http://lv2plug.in/ns/lv2core#")
    @pg   = RDF::Vocabulary.new(@port_groups_uri)
    @doap = RDF::Vocabulary.new("http://usefulinc.com/ns/doap#")
    @atom = RDF::Vocabulary.new("http://lv2plug.in/ns/ext/atom#")
    @midi = RDF::Vocabulary.new("http://lv2plug.in/ns/ext/midi#")
    @foaf = RDF::Vocabulary.new("http://xmlns.com/foaf/0.1/")

    @plugin_classes = get_plugin_classes
    @port_groups = get_port_groups
  end

  def group_supported(group_name)
    @port_groups.each do |g, members|
      return true if g.to_s.sub(@port_groups_uri, "") == group_name
    end
    return false
  end

  def group_member_supported(group_name, group_member)
    @port_groups.each do |g, members|
      if g.to_s.sub(@port_groups_uri, "") == group_name
        members.each do |m|
          return true if m.to_s.sub(@port_groups_uri, "") == group_member
        end
        return false
      end
    end
    return false
  end

  def group_validate(group_name, group_members)
    @port_groups.each do |g, members|
      if g.to_s.sub(@port_groups_uri, "") == group_name
        members.each do |m|
          m = m.to_s.sub(@port_groups_uri, "")
          unless group_members.include?(m)
            raise "group #{group_name} missing member #{m}"
          end
        end
      end
    end
    return true
  end

  private
  def find_lv2_dir
    [ENV['LV2_INCLUDE_DIR'], '/usr/local/include/lv2', '/usr/include/lv2'].each do |dir|
      next unless dir
      p = File.join(dir, "lv2plug.in/")
      if File.directory?(p)
        @lv2_include_dir = p
        break
      end
    end
  end

  def get_plugin_classes
    f = File.join(@lv2_include_dir, "/ns/lv2core/lv2core.ttl")
    g = RDF::Graph.load(f)
    types = [ RDF::URI.new("http://lv2plug.in/ns/lv2core#Plugin") ]
    types.each do |t|
      g.query([nil, @rdf_subclass, t]).each do |s|
        types << s[0]
      end
    end
    return types.uniq
  end

  def get_port_groups
    f = File.join(@lv2_include_dir, "/ns/ext/port-groups/port-groups.ttl")
    g = RDF::Graph.load(f)
    types = [ RDF::URI.new("http://lv2plug.in/ns/ext/port-groups#Group") ]
    types.each do |t|
      g.query([nil, @rdf_subclass, t]).each do |s|
        types << s[0]
      end
    end
    types.uniq!
    data = {}
    types.each do |t|
      elements = []
      g.query([t, @pg.element, nil]).each do |s|
        g.query([s[2], @lv2.designation, nil]).each do |s|
          elements << s[2]
        end
      end
      data[t] = elements
    end
    return data
  end
end

=begin
  x = LV2.new
  puts x.lv2_include_dir

  puts "\nGROUPS:"
  x.port_groups.each do |name, elements|
    puts "#{name} => #{elements.join(', ')}"
  end

  puts "\nCLASSES:"
  puts x.plugin_classes.join("\n")
=end
