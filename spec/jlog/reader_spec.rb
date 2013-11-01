require 'spec_helper'

describe "Jlog::Reader" do
  let(:jlog)   { Jlog.new('/tmp/junit.log') }
  let(:reader) { Jlog::Reader.new('/tmp/junit.log') }

  before do
    jlog.add_subscriber('TestSub')
    writer = Jlog::Writer.new('/tmp/junit.log')

    writer.open
    1.upto(10) do |n|
      writer.write("Test Unit #{n}")
    end
    writer.close
  end

  it "should be able to access a reader subscribed to a log" do
    reader.open('TestSub')
    reader.close
  end

  it "should read a hash" do
    reader.open('TestSub')
    msg = reader.read_message
    reader.checkpoint

    assert_kind_of String, msg[:message]
    assert_kind_of Float, msg[:timestamp]
  end

  it "should read from the proper checkpoint" do
    reader.open('TestSub')
    first_entry = reader.read
    reader.close

    reader = Jlog::Reader.new('/tmp/junit.log')
    reader.open('TestSub')
    assert_equal first_entry, reader.read, "Message wasn't appropriately checkpointed"
    reader.close
  end

  it "should be able to rewind" do
    reader.open('TestSub')
    res1 = reader.read
    reader.rewind
    res2 = reader.read

    assert_equal res1, res2, "Messages do not match"

    reader.checkpoint
    reader.close
  end

  it "should be able to checkpoint" do
    reader.open('TestSub')
    res1 = reader.read
    reader.checkpoint
    reader.rewind
    res2 = reader.read

    refute_equal res1, res2, "Checkpointed messages should not match"

    reader.close
  end
end
