require "bundler/gem_tasks"
require "rake/extensiontask"

Rake::ExtensionTask.new("jlog")

task :test do
  $LOAD_PATH.unshift('lib', 'spec')
  Dir.glob('./spec/**/*_spec.rb') { |f| require f }
end

task :default => :test
