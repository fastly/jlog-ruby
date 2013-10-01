require 'spec_helper'

describe 'JLog::Writer' do
  let(:writer) { JLog::Writer.new('/tmp/junit.log') }

  it "it should be able to open a log for writing" do
    assert_kind_of JLog::Writer, writer, "JLog::Writer Object creation failed"
    writer.open
    writer.close
  end

  it "should be able to wrote to an open log" do
    writer.open

    1.upto(10) do |n|
      writer.write("Test Unit #{n}")
    end

    writer.close
  end
end
