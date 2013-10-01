require 'spec_helper'

describe 'JLog' do
  let(:jlog) { JLog.new('/tmp/junit.log') }

  it "should work" do
    assert_kind_of JLog, jlog, "JLog Object creation failed"
    jlog.close
  end

  it "should be able to add subscribers" do
    jlog.add_subscriber("TestSub")
    jlog.add_subscriber("TestSubRemove")

    count = 2

    jlog.list_subscribers.each do |s|
      if s =~ /TestSub(Remove|)/
        count -= 1
      end
    end

    assert_equal 0, count
    jlog.close
  end

  it "should be able to remove subscribers" do
    jlog.remove_subscriber("TestSubRemove")
    jlog.close

    jlog = JLog.new('/tmp/junit.log')
    jlog.list_subscribers.each do |s|
      refute_equal "TestSubRemove", s, "Test Subscriber was not removed"
    end
    jlog.close
  end
end
