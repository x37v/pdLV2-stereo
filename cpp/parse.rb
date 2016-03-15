require 'rdf'
require 'linkeddata'

plugins = ["patch.pd"]

controlInRegex = /r(?:eceive){0,1}\s*\\\$0-lv2-(.*);\s*/
controlOutRegex = /s(?:end){0,1}\s*\\\$0-lv2-(.*);\s*/

def get_control_data(content)
  floatRegex = /\d+(?:\.\d+)?/
  controlLabelRegex = /label:\s*(\w*)/
  rangeRegex = /range:\s+(#{floatRegex})\s*(#{floatRegex})\s*(#{floatRegex})/

  data = {}
  data[:symbol] = content.match(/\A(\w+)/)[0]
  data[:label] =
    if content =~ controlLabelRegex
      $1
    end
  data[:range] = 
    if content =~ rangeRegex
      [$1, $3, $2]
    end
  return data
end

plugins.each do |p|
  input = 0
  output = 0
  name = nil
  uri = nil
  in_controls = []
  out_controls = []

  File.open(p) do |f|
    f.readlines.each do |l|
      if l =~ /dac~\s(.*);\s*/
        $1.scan(/\d+/).each do |d|
          output = d.to_i if d.to_i > output
        end
      elsif l =~ /adc~\s(.*);\s*/
        $1.scan(/\d+/).each do |d|
          input = d.to_i if d.to_i > input
        end
      elsif l =~ controlInRegex
        in_controls << get_control_data($1)
      elsif l =~ controlOutRegex
        out_controls << get_control_data($1)
      elsif l =~ /pluginURI:\s(.*);\s*/
        uri = $1
      elsif l =~ /pluginName:\s(.*);\s*/
        name = $1
      end
    end

    raise "need uri" unless uri
    raise "need name" unless name

    puts "name: #{name}"
    puts "uri: #{uri}"
    puts "audio inputs: #{input}"
    puts "audio outputs: #{output}"
    puts "control inputs: #{in_controls.size}"
    puts "control outputs: #{out_controls.size}"
  end
end
