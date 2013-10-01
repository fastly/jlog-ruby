require 'spec_helper'

describe "JLog::Reader" do
  let(:jlog)   { JLog.new('/tmp/junit.log') }
  let(:reader) { JLog::Reader.new('/tmp/junit.log') }

  before do
    jlog.add_subscriber('TestSub')
    writer = JLog::Writer.new('/tmp/junit.log')

    writer.open
    1.upto(10) do |n|
      writer.write("Test Unit #{n}")
    end
    writer.close
  end

  it "should be able to open a reader on a log" do
    reader.open('TestSub')
    reader.close
  end

  it "should read from the proper checkpoint" do
    reader.open('TestSub')
    first_entry = reader.read
    reader.close

    reader = JLog::Reader.new('/tmp/junit.log')
    reader.open('TestSub')
    assert_equal first_entry, reader.read, "Message wasn't appropriately checkpointed"
    reader.close
  end

  it "should be able to rewind" do
    reader.open('TestSub')
    res1 = reader.read
    reader.checkpoint
    res2 = reader.read

    refute_equal res1, res2, "Checkpointed messages should not match"

    reader.checkpoint
    reader.close
  end
end
