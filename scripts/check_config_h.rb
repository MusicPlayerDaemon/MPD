#!/usr/bin/env ruby
#
# This script verifies that every source includes config.h first.
# This is very important for consistent Large File Support.
#

def check_file(file)
    first = true
    file.each_line do |line|
        if line =~ /^\#include\s+(\S+)/ then
            if $1 == '"config.h"'
                unless first
                    puts "#{file.path}: config.h included too late"
                end
            else
                if first
                    puts "#{file.path}: config.h missing"
                end
            end
            first = false
        end
    end
end

def check_path(path)
    File.open(path) do |file|
        check_file(file)
    end
end

if ARGV.empty?
    Dir["src/*.c"].each do |path|
        check_path(path)
    end

    Dir["src/*/*.c"].each do |path|
        check_path(path)
    end

    Dir["test/*.c"].each do |path|
        check_path(path)
    end
else
    ARGV.each do |path|
        check_path(path)
    end
end
