# JLog

A Ruby C-extension for OmniTI Labs' [JLog](https://github.com/omniti-labs/jlog). Leverages [work](https://github.com/mbainter/ruby-jlog/) by Mark Bainter (@mbainter.)

> JLog is short for "journaled log" and this package is really an API and implementation that is libjlog. What is libjlog? libjlog is a pure C, very simple durable message queue with multiple subscribers and publishers (both thread and multi-process safe). The basic concept is that publishers can open a log and write messages to it while subscribers open the log and consume messages from it. "That sounds easy." libjlog abstracts away the need to perform log rotation or maintenance by publishing into fixed size log buffers and eliminating old log buffers when there are no more consumers pending.

## Installation

Add this line to your application's Gemfile:

    gem 'jlog'

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install jlog

## Usage

```ruby
log = JLog.new('/var/log/logname')
log.add_subscriber 'LogSubscriber'
log.close

writer = JLog::Writer.new('/var/log/logname')

writer.open
writer.write 'This is the first log message'
writer.write 'This is the second log message'
writer.close

reader = JLog::Reader.new '/var/log/logname'
reader.open 'LogSubscriber'
first = reader.read
second = reader.read
reader.rewind

if reader.read == second
  puts "Yeah, dude!"
else
  puts "No, bro!"
end

reader.close
```

## Requirements

* JLog
