require File.dirname(File.join(__rhoGetCurrentDir(), __FILE__)) + '/../../spec_helper'

describe "FalseClass#|" do
  it "return false if other is nil or false, otherwise true" do
    (false | false).should == false
    (false | true).should == true
    (false | nil).should == false
    (false | "").should == true
    (false | mock('x')).should == true
  end
end
