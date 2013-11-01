require "bundler/gem_tasks"
require "rake/extensiontask"

spec = Gem::Specification.new("jlog.gemspec")

task :test => [:clobber, :compile] do
  $LOAD_PATH.unshift('lib', 'spec')
  Dir.glob('./spec/**/*_spec.rb') { |f| require f }
end

task :default => :test
task :spec => :test

Rake::ExtensionTask.new do |ext|
  ext.name = "jlog"
  ext.ext_dir = "ext/jlog"
  ext.lib_dir = "lib/jlog"
  ext.tmp_dir = "tmp"
  ext.source_pattern = "*.c"
  ext.config_options << '-Wall'
  ext.gem_spec = spec
end
