require 'rdf'
require 'linkeddata'

@objRegex = /#X obj \d+ \d+ /
@msgRegex = /#X msg \d+ \d+ /
@controlInRegex = /#{@objRegex}r(?:eceive){0,1}\s*\\\$0-lv2-(.*);\s*/
@controlOutRegex = /#{@objRegex}s(?:end){0,1}\s*\\\$0-lv2-(.*);\s*/
@controlLabelRegex = /label:\s*(\w*)/
@floatRegex = /\d+(?:\.\d+)?/
@rangeRegex = /range:\s+(#{@floatRegex})\s*(#{@floatRegex})\s*(#{@floatRegex})/

def get_control_data(content)
  data = {}
  data[:symbol] = content.match(/\A(\w+)/)[0]
  data[:label] =
    if content =~ @controlLabelRegex
      $1
    end
  data[:range] = 
    if content =~ @rangeRegex
      [$1, $2, $3]
    end
  return data
end

def print_control(data)
  r = data[:range]
  puts "\t#{data[:symbol]}"
  puts "\t\tlabel: #{data[:label]}" if data[:label]
  puts "\t\trange: #{r.join(', ')}" if r
end

def parse_pd_file(patch_path)
  input = 0
  output = 0
  name = nil
  uri = nil
  in_controls = []
  out_controls = []

  File.open(patch_path) do |f|
    f.readlines.each do |l|
      if l =~ /#{@objRegex}dac~\s(.*);\s*/
        $1.scan(/\d+/).each do |d|
          output = d.to_i if d.to_i > output
        end
      elsif l =~ /#{@objRegex}adc~\s(.*);\s*/
        $1.scan(/\d+/).each do |d|
          input = d.to_i if d.to_i > input
        end
      elsif l =~ @controlInRegex
        in_controls << get_control_data($1)
      elsif l =~ @controlOutRegex
        out_controls << get_control_data($1)
      elsif l =~ /#{@msgRegex}pluginURI:\s(.*);\s*/
        uri = $1
      elsif l =~ /#{@msgRegex}pluginName:\s(.*);\s*/
        name = $1
      end
    end

    raise "need uri" unless uri
    raise "need name" unless name
    raise "need at least one control or audio input or output" unless input + output + in_controls.size + out_controls.size > 0


    puts "name: #{name}"
    puts "uri: #{uri}"
    puts "audio inputs: #{input}"
    puts "audio outputs: #{output}"
    if in_controls.size
      puts "control inputs:"
      in_controls.each do |c|
        print_control(c)
      end
    end
    if out_controls.size
      puts "control outputs:"
      out_controls.each do |c|
        print_control(c)
      end
    end
  end
end


plugins = ["patch.pd"]
plugins.each do |p|
  parse_pd_file(p)
end
