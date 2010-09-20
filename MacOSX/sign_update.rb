#!/usr/bin/ruby
if ARGV.length < 2
  puts "Usage: ruby sign_update.rb update_archive private_key"
  exit
end

puts `/usr/bin/openssl dgst -sha1 -binary < "#{ARGV[0]}" | /usr/bin/openssl dgst -dss1 -sign "#{ARGV[1]}" | /usr/bin/openssl enc -base64`
