# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'jlog/version'

Gem::Specification.new do |spec|
  spec.name          = "jlog"
  spec.version       = Jlog::VERSION
  spec.authors       = ["Ezekiel Templin"]
  spec.email         = ["zeke@templ.in"]
  spec.description   = %q{Ruby C-extension for JLog}
  spec.summary       = %q{A Ruby C-extenion for using JLog}
  spec.homepage      = "https://github.com/ezkl/jlog-ruby"
  spec.license       = "MIT"

  spec.files         = `git ls-files`.split($/)
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.extensions    = ['ext/jlog/extconf.rb']
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["lib", "ext"]

  spec.add_development_dependency "bundler", "~> 1.3"
  spec.add_development_dependency "rake"
  spec.add_development_dependency "rake-compiler"
  spec.add_development_dependency "minitest", "~> 5.0.8"
end
