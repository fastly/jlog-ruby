require 'bundler/setup'
require 'minitest/autorun'

#require File.expand_path('../../lib/jlog', __FILE__)
require 'jlog'

class Minitest::Spec
  after do
    if File.exists?('/tmp/junit.log')
      system 'rm -rf /tmp/junit.log'
    end
  end
end
