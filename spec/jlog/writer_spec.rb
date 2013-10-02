require 'spec_helper'

describe 'Jlog::Writer' do
  let(:writer) { Jlog::Writer.new('/tmp/junit.log') }

  it "it should be able to open a log for writing" do
    assert_kind_of Jlog::Writer, writer, "Jlog::Writer Object creation failed"
    writer.open
    writer.close
  end

  it "should be able to write to an open log" do
    writer.open

    1.upto(10) do |n|
      writer.write("Test Unit #{n}")
    end

    writer.close
  end
end
