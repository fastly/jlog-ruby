require 'spec_helper'

describe 'Jlog' do
  let(:jlog) { Jlog.new('/tmp/junit.log') }

  it "should work" do
    assert_kind_of Jlog, jlog, "JLog Object creation failed"
    jlog.close
  end

  it "should be able to add subscribers" do
    jlog.add_subscriber("TestSub")
    jlog.add_subscriber("TestSubRemove")

    subscribers = jlog.list_subscribers
    subscribers.delete_if do |sub|
      sub =~ /^TestSub/
    end

    assert_equal 0, subscribers.size
    jlog.close
  end

  it "should be able to remove subscribers" do
    jlog.remove_subscriber("TestSubRemove")
    jlog.close

    jlog = Jlog.new('/tmp/junit.log')
    jlog.list_subscribers.each do |s|
      refute_equal "TestSubRemove", s, "Test Subscriber was not removed"
    end
    jlog.close
  end

  it "should be able to see subscribers added by others" do
    assert_equal 0, jlog.list_subscribers.size
    new_jlog = Jlog.new('/tmp/junit.log')
    new_jlog.add_subscriber('NewSubscriber')
    assert_equal 1, jlog.list_subscribers.size
    assert_equal 1, new_jlog.list_subscribers.size
  end
end
