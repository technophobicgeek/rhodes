require File.dirname(File.join(__rhoGetCurrentDir(), __FILE__)) + '/../../spec_helper'

describe "SystemExit#initialize" do
  it "accepts a status" do
    s = SystemExit.new 1
    s.status.should == 1
    s.message.should == 'SystemExit'
  end

  it "accepts a message" do
    s = SystemExit.new 'message'
    s.status.should == 0
    s.message.should == 'message'
  end

  it "accepts a status and message" do
    s = SystemExit.new 10, 'message'
    s.status.should == 10
    s.message.should == 'message'
  end
end

