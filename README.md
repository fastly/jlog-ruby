# JLog

Ruby C-bindings for OmniTI Labs' [Jlog](https://github.com/omniti-labs/jlog). Founded upon [work](https://github.com/mbainter/ruby-jlog/) by Mark Bainter (@mbainter.)

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
log = Jlog.new('/var/log/logname')
log.add_subscriber 'LogSubscriber'
log.close

writer = Jlog::Writer.new('/var/log/logname')

writer.open
writer.write 'This is the first log message'
writer.write 'This is the second log message'
writer.write "This is the third log message, created at #{Time.now}"
writer.close

reader = Jlog::Reader.new '/var/log/logname'
reader.open 'LogSubscriber'

first = reader.read
second = reader.read
reader.rewind

if reader.read == second
  puts "Rewind sets log position to last checkpoint."
end

reader.checkpoint

third = reader.read
reader.rewind
third_full = reader.read_message

if third == third_msg[:message]
  ts = third_msg[:timestamp]
  puts "#{third} and logged #{Time.at(ts)} (or #{ts} seconds since epoch)"
end

reader.checkpoint
reader.close
```

## Requirements

* [Jlog](https://github.com/omniti-labs/jlog)
