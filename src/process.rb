#!/usr/bin/env ruby
=begin

Copyright (c) Alex Norman, 2016.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

=end

require 'rdf'
require 'rdf/ntriples'
require 'linkeddata'
require 'fileutils'

require_relative './lv2groups'

#arguments: source_pd_patch destination_directory

DEFAULT_LICENSE = 'http://usefulinc.com/doap/licenses/gpl'

@objRegex = /#X obj (\d+) (\d+)\s+/ #obj x y
@msgRegex = /#X msg \d+ \d+\s+/

@controlInRegex = /#{@objRegex}r(?:eceive){0,1}\s*\\\$1-lv2-(.*);\s*/
@controlOutRegex = /#{@objRegex}s(?:end){0,1}\s*\\\$1-lv2-(.*);\s*/

@audioInRegex = /#{@objRegex}inlet~\s*(.*?);\s*/
@audioOutRegex = /#{@objRegex}outlet~\s*(.*?);\s*/

@labelRegex = /label:\s*([\w-]*)/
@groupRegex = /group:\s*([\w:-]*)/
@groupFormatRegex = /\A([\w-]*):([\w]*):([\w]*)\z/
@floatRegex = /-?\d+(?:\.\d+)?/
@rangeRegex = /range:\s+(#{@floatRegex})\s*(#{@floatRegex})\s*(#{@floatRegex})/

PD_MIDI_OBJ = {
  :in => [
    "notein",
    "ctlin",
    "pgmin",
    "bendin",
    "touchin",
    "polytouchin",
    "midiin",
    "sysexin",
  ],
  :out => [
    "noteout",
    "pgmout",
    "bendout",
    "ctlout",
    "touchout",
    "polytouchout",
    "midiout",
  ]
}

def get_control_data(content)
  data = {}
  data[:symbol] = content.match(/\A(\w+)/)[0]
  data[:label] =
    if content =~ @labelRegex
      $1
    else
      data[:symbol]
    end
  data[:range] = 
    if content =~ @rangeRegex
      [$1.to_f, $2.to_f, $3.to_f]
    end
  return data
end

def get_audio_data(x, y, content)
  data = {:x => x.to_i}
  if content =~ @groupRegex
    g = $1
    if g =~ @groupFormatRegex
    else
      raise "#{g} not a valid group format: name:GroupClass:role"
    end
    data[:group] = {:name => $1, :type => $2, :member => $3}
    data[:label] = data[:group][:name] + ":" + data[:group][:member]
  end
  if content =~ @labelRegex
    data[:label] = $1
  elsif not data[:label]
    raise "audio inlets/outlets need to have a label or group"
  end
  return data
end

def consolidate_pd_lines(lines)
  out = []
  #unwrap wrapped lines
  lines.each do |l|
    #pd lines start with # unless they're a continuation
    unless l =~ /\A#/ or out.size == 0
      l = out.pop + l
    end
    out << l.chomp
  end
  return out
end

@subpatchOpen = /\A#N canvas \d+ \d+ \d+ \d+ (\w+).*?;/
@subpatchClose = /\A#X restore \d+ \d+ pd (\w+).*?;/

def test_and_extract_groups(audio_ports)
  groups = {}

  #find the groups
  audio_ports.each do |p|
    g = p[:group]
    next unless g
    name = g[:name]
    type = g[:type]
    member = g[:member]
    raise "unsupported group type #{type}" unless LV2Groups::SUPPORTED[type]
    raise "unsupported group #{type} member #{member}" unless LV2Groups::SUPPORTED[type][member]

    groups[name] = {:type => type, :members => [], :dir => p[:dir]} unless groups[name]
    raise "duplicate member #{member} in #{type} group #{name}" if groups[name][:members].include?(member)
    raise "#{type} group #{name} has both in and out ports" if groups[name][:dir] != p[:dir]
    groups[name][:members] << member
  end

  names = {}
  #validate
  groups.each do |name, data|
    LV2Groups::SUPPORTED[data[:type]].each do |member, range|
      unless data[:members].include?(member) or range.include?(0)
        raise "group #{name} missing member of type #{member}"
      end
    end
    names[name] = data[:type]
  end
  return names
end

def parse_pd_file(patch_path)
  audio_in = []
  audio_out = []
  name = nil
  maintainer = nil
  uri = nil
  in_controls = []
  out_controls = []
  in_midi = []
  out_midi = []
  license = DEFAULT_LICENSE

  File.open(patch_path) do |f|
    lines = consolidate_pd_lines(f.readlines)

    subpatch_open = {}
    lines.each_with_index do |l, line_num|
      raise "dac~ not supported" if l =~ /#{@objRegex}dac~/
      raise "adc~ not supported" if l =~ /#{@objRegex}adc~/

      #look for subpatches
      if line_num != 0
        if l =~ @subpatchOpen
          subpatch_open[$1] = (subpatch_open[$1] || 0) + 1
        elsif l =~ @subpatchClose
          raise "subpatch parse fail #{$1}" unless subpatch_open[$1]
          if subpatch_open[$1] <= 1
            subpatch_open.delete($1)
          else
            subpatch_open[$1] = subpatch_open[$1] - 1
          end
        end
      end
      in_subpatch = subpatch_open.size > 0

      if l =~ /#{@objRegex}inlet[^~]*;/ and not in_subpatch
        raise "control inlets not supported in plugin top level"
      end
      if l =~ /#{@objRegex}outlet[^~]*;/ and not in_subpatch
        raise "control outlets not supported in plugin top level"
      end

      if l =~ @audioOutRegex
        next if in_subpatch #don't do inlet~ or outlet~ in subpatch
        begin
          audio_out << get_audio_data($1, $2, $3)
        rescue => e
          raise "problem with #{l} #{e}"
        end
      elsif l =~ @audioInRegex
        next if in_subpatch #don't do inlet~ or outlet~ in subpatch
        begin
          audio_in << get_audio_data($1, $2, $3)
        rescue => e
          raise "problem with #{l}\n#{e}"
        end
      elsif l =~ @controlInRegex
        in_controls << get_control_data($3)
      elsif l =~ @controlOutRegex
        out_controls << get_control_data($3)
      elsif l =~ /#{@msgRegex}pluginURI:\s(.*);\s*/
        uri = $1
      elsif l =~ /#{@msgRegex}pluginName:\s(.*);\s*/
        name = $1
      elsif l =~ /#{@msgRegex}pluginLicense:\s(.*);\s*/
        license = $1
      elsif l =~ /#{@msgRegex}pluginMaintainer:\s(.*);\s*/
        maintainer = $1
      else
        PD_MIDI_OBJ[:in].each do |obname|
          in_midi << obname if l =~ /\A#X obj \d+ \d+ #{obname}/
        end
        PD_MIDI_OBJ[:out].each do |obname|
          out_midi << obname if l =~ /\A#X obj \d+ \d+ #{obname}/
        end
      end
    end

    raise "need uri" unless uri
    raise "need name" unless name
    unless (audio_in.size + audio_out.size +
      in_controls.size + out_controls.size +
      in_midi.size + out_midi.size) > 0
      raise "need at least one control or audio input or output"
    end

    outdata = {
      :name => name,
      :uri => uri,
      :license => license
    }
    outdata[:maintainer] = maintainer if maintainer
    audio_in = audio_in.sort_by { |info| info[:x] }
    audio_out = audio_out.sort_by { |info| info[:x] }

    begin
      audio = audio_in.collect { |p| p.merge({:dir => :in}) } + audio_out.collect { |p| p.merge({:dir => :out}) }
      outdata[:groups] = test_and_extract_groups(audio)
    rescue => e
      raise "problem with audio groups #{e}"
    end

    outdata[:audio_in] = audio_in
    outdata[:audio_out] = audio_out
    outdata[:control_in] = in_controls if in_controls.size
    outdata[:control_out] = out_controls if out_controls.size
    outdata[:midi_in] = in_midi
    outdata[:midi_out] = out_midi
    return outdata
  end
end

def print_control(data)
  r = data[:range]
  puts "\t#{data[:symbol]}"
  puts "\t\tlabel: #{data[:label]}" if data[:label]
  puts "\t\trange: #{r.join(', ')}" if r
end

def audio_port_info(info, index, direction)
  l = info[:label]
  g = info[:group]
  s = l.gsub('-', '_').gsub(":", "_").downcase
  return {:type => :audio, :dir => direction, :symbol => s, :label => l, :group => g}
end

#audio in, out, control in, out
def ports(data)
  p = []
  data[:audio_in].each_with_index do |a, i|
    p << audio_port_info(a, i, :in)
  end
  data[:audio_out].each_with_index do |a, i|
    p << audio_port_info(a, i, :out)
  end
  data[:control_in].each do |c|
    p << c.merge({:type => :control, :dir => :in})
  end
  data[:control_out].each do |c|
    p << c.merge({:type => :control, :dir => :out})
  end
  if data[:midi_in].size > 0
    p << {:type => :midi, :dir => :in, :symbol => 'midi_in', :label => 'Midi In'}
  end
  if data[:midi_out].size > 0
    p << {:type => :midi, :dir => :out, :symbol => 'midi_out', :label => 'Midi Out'}
  end

  return p
end

def print_plugin(data)
  puts "name: #{data[:name]}"
  puts "uri: #{data[:uri]}"
  puts "license: #{data[:license]}"
  puts "audio inputs: #{data[:audio_in]}"
  puts "audio outputs: #{data[:audio_out]}"

  if data[:control_in].size != 0
    puts "control inputs:"
    data[:control_in].each do |c|
      print_control(c)
    end
  end

  if data[:control_out].size != 0
    puts "control outputs:"
    data[:control_out].each do |c|
      print_control(c)
    end
  end

  if data[:midi_in].size != 0
    puts "midi inputs:"
    puts "\t" + data[:midi_in].join(", ")
  end
  if data[:midi_out].size != 0
    puts "midi outputs:"
    puts "\t" + data[:midi_out].join(", ")
  end
end

@lv2 = RDF::Vocabulary.new("http://lv2plug.in/ns/lv2core#")
@pg = RDF::Vocabulary.new("http://ll-plugins.nongnu.org/lv2/ext/portgroups#")
@doap = RDF::Vocabulary.new("http://usefulinc.com/ns/doap#")
@atom = RDF::Vocabulary.new("http://lv2plug.in/ns/ext/atom#")
@midi = RDF::Vocabulary.new("http://lv2plug.in/ns/ext/midi#")
@foaf = RDF::Vocabulary.new("http://xmlns.com/foaf/0.1/")

=begin

#query for all the plugin types?
g = RDF::Graph.load("/usr/lib/lv2/lv2core.lv2/lv2core.ttl")
g.query([nil, RDF::URI.new("http://www.w3.org/2000/01/rdf-schema#subClassOf"), RDF::URI.new("http://lv2plug.in/ns/lv2core#Plugin")]).each { |s| puts s }

=end

@rdf_prefixes = {
  rdfs: "http://www.w3.org/2000/01/rdf-schema#",
  rdf:  "http://www.w3.org/1999/02/22-rdf-syntax-ns#",
  xsd:  "http://www.w3.org/2001/XMLSchema#",
  lv2:  @lv2.to_iri.to_s,
  doap: @doap.to_iri.to_s,
  atom: @atom.to_iri.to_s,
  midi: @midi.to_iri.to_s,
  pg:   @pg.to_iri.to_s,
  foaf: @foaf.to_iri.to_s,
}

def write_rdf(data, path)
  details_file = "details.ttl"
  manifest_file = "manifest.ttl"

  group_name_to_uri = {}

  uri = RDF::URI.new(data[:uri])

  manifest = RDF::Graph.new
  manifest << [uri, RDF.type, @lv2.Plugin]
  manifest << [uri, RDF::RDFS.seeAlso, RDF::URI.new(details_file)]

  details = RDF::Graph.new

  details << [uri, RDF.type, @lv2.Plugin]
  details << [uri, @lv2.binary, RDF::URI.new(data[:binary])]
  details << [uri, @doap.name, data[:name]]
  details << [uri, @doap.license, RDF::URI.new(data[:license])]
  details << [uri, @lv2.requiredFeature, RDF::URI.new("http://lv2plug.in/ns/ext/urid#map")]

  if data[:maintainer]
    node = RDF::Node.new
    details << [uri, @doap.maintainer, node]
    details << [node, @foaf.name, data[:maintainer]]
  end

  data[:groups].each do |name, type|
    group_uri = RDF::URI.new(File.join(data[:uri], name))
    group_name_to_uri[name] = group_uri
    details << [group_uri, RDF.type, @pg[type.to_s]]
  end

  ports(data).each_with_index do |p, i|
    node = RDF::Node.new
    details << [uri, @lv2.port, node]
    details << [node, @lv2.index, i]

    if p[:type] == :midi
      details << [node, RDF.type, @atom.AtomPort]
      details << [node, @atom.bufferType, @atom.Sequence]
      details << [node, @atom.supports, @midi.MidiEvent]
      details << [node, @lv2.designation, @lv2.control]
    elsif p[:type] == :audio 
      details << [node, RDF.type, @lv2.AudioPort]
    elsif p[:type] == :control 
      details << [node, RDF.type, @lv2.ControlPort]
    else
      raise "#{p[:type]} is not a supported pdlv2 port type"
    end

    details << [node, RDF.type, @lv2.InputPort] if p[:dir] == :in
    details << [node, RDF.type, @lv2.OutputPort] if p[:dir] == :out

    details << [node, @lv2.symbol, p[:symbol]]
    details << [node, @lv2.name, p[:label]]

    if p[:group]
      details << [node, @pg.group, group_name_to_uri[p[:group][:name]]]
      details << [node, @lv2.designation, @pg[p[:group][:member]]]
    end

    if p[:range]
      details << [node, @lv2.minimum, p[:range][0]]
      details << [node, @lv2.maximum, p[:range][2]]
      details << [node, @lv2.default, p[:range][1]]
    end
  end

  FileUtils.mkdir_p(path)
  manifest_file = File.join(path, manifest_file)
  puts "writing #{manifest_file}"
  RDF::Turtle::Writer.open(manifest_file, :prefixes => @rdf_prefixes) do |writer|
    writer << manifest
  end

  details_file = File.join(path, details_file)
  puts "writing #{details_file}"
  RDF::Turtle::Writer.open(details_file, :prefixes => @rdf_prefixes) do |writer|
    writer << details
  end
end

def write_header(data, path)
  file_path = File.join(path, "plugin.h")
  puts "writing #{file_path}"
  File.open(file_path, "w") do |f|
    f.puts '#include "defines.h"'
    f.puts "\n"
    f.puts 'namespace pdlv2 {'
    f.puts "  const char * plugin_uri = \"#{data[:uri]}\";"

    f.puts '  const std::vector<pdlv2::PortInfo> ports = {';
    ports(data).each do |p|
      line = '{'
      if p[:type] == :audio
        line = line + (p[:dir] == :out ? "AUDIO_OUT" : "AUDIO_IN")
      elsif p[:type] == :control
        line = line + (p[:dir] == :out ? "CONTROL_OUT" : "CONTROL_IN")
      elsif p[:type] == :midi
        line = line + (p[:dir] == :out ? "MIDI_OUT" : "MIDI_IN")
      else
        raise "#{p[:type]} not supported"
      end
      line = line + ", \"#{p[:symbol]}\"},"
      f.puts '    ' + line
    end
    f.puts '  };'
    f.puts '}'
  end
end

@dacRegex = /(#X obj \d+ \d+ dac~\s*).*?;\s*/
@adcRegex = /(#X obj \d+ \d+ adc~\s*).*?;\s*/

def new_audio_line(prefix, audio_array)
  return prefix + audio_array.size.times.collect { |i| (i + 1).to_s }.join(" ") + ";"
end

def connect(obout, outlet, obin, inlet)
  "#X connect #{obout} #{outlet} #{obin} #{inlet};"
end

def rewrite_host(data, host_in, path)
  host = consolidate_pd_lines(File.open(host_in).readlines)
  out_host = []
  obj_count = 0
  obj_indx = {}

  #remove or rewrite dac~ and adc~ with correct args
  host.each do |h|
    if h =~ @dacRegex
      if data[:audio_out].size != 0
        out_host << new_audio_line($1, data[:audio_out])
        obj_indx[:out] = obj_count
      else
        out_host << h #keep it, shouldn't hurt anything and keeps obj count
      end
    elsif h =~ @adcRegex
      if data[:audio_in].size != 0
        out_host << new_audio_line($1, data[:audio_in])
        obj_indx[:in] = obj_count
      else
        out_host << h #keep it, shouldn't hurt anything and keeps obj count
      end
    elsif h =~ /#X obj.*plugin/
      obj_indx[:plugin] = obj_count 
      out_host << h
    else
      out_host << h
    end
    obj_count = obj_count + 1 if h =~ @objRegex
  end
  raise "cannot find [plugin] in host" unless obj_indx[:plugin]

  #write the connections
  if obj_indx[:in]
    data[:audio_in].size.times do |i|
      out_host << connect(obj_indx[:in], i, obj_indx[:plugin], i)
    end
  end
  if obj_indx[:out]
    data[:audio_out].size.times do |i|
      out_host << connect(obj_indx[:plugin], i, obj_indx[:out], i)
    end
  end

  file_path = File.join(path, "host.pd")
  puts "writing #{file_path}"
  File.open(file_path, "w") do |f|
    out_host.each do |h|
      f.puts h
    end
  end
end

source = ARGV[0]
dest = ARGV[1]

puts "source: #{source} dest #{dest}"

data = parse_pd_file(source)

#make sure we don't have duplicate port labels
#XXX slow for long lists
port_labels = ports(data).collect { |p| p[:label] }
dups = port_labels.select{ |item| port_labels.count(item) > 1}.uniq
if dups.size > 0
  puts "duplicate port labels aren't allowed: #{dups.join(', ')}"
  exit -1
end

#print_plugin(data)
data[:binary] = "pdlv2.so"
write_rdf(data, dest)
write_header(data, dest)
rewrite_host(data, "src/host.pd", dest)
