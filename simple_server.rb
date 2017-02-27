#!/usr/bin/env ruby
#
require 'socket'

server = TCPServer.new 5000
loop do
  client = server.accept
  client.puts "Simple Serial Concentrator"
  while line = client.gets.chomp do
    client.puts "You said: [#{line}]."
  end
  client.close
end
