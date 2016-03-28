#!/usr/bin/env ruby

require 'rdf'
require 'linkeddata'

sc = RDF::URI.new("http://www.w3.org/2000/01/rdf-schema#subClassOf")

g = RDF::Graph.load("/usr/lib/lv2/lv2core.lv2/lv2core.ttl")
types = [ RDF::URI.new("http://lv2plug.in/ns/lv2core#Plugin") ]
types.each do |t|
  g.query([nil, sc, t]).each do |s|
    types << s[0]
  end
end
types.uniq!
puts types

g = RDF::Graph.load("/usr/lib/lv2/port-groups.lv2/port-groups.ttl")
types = [ RDF::URI.new("http://lv2plug.in/ns/ext/port-groups#Group") ]
types.each do |t|
  g.query([nil, sc, t]).each do |s|
    types << s[0]
  end
end
types.uniq!
puts types
