require File.dirname(File.join(__rhoGetCurrentDir(), __FILE__)) + '/../../spec_helper'
require File.dirname(File.join(__rhoGetCurrentDir(), __FILE__)) + '/fixtures/classes.rb'

describe "String#<=> with String" do
  it "compares individual characters based on their ascii value" do
    ascii_order = Array.new(256) { |x| x.chr }
    sort_order = ascii_order.sort
    sort_order.should == ascii_order
  end
  
  it "returns -1 when self is less than other" do
    ("this" <=> "those").should == -1
  end

  it "returns 0 when self is equal to other" do
    ("yep" <=> "yep").should == 0
  end

  it "returns 1 when self is greater than other" do
    ("yoddle" <=> "griddle").should == 1
  end
  
  it "considers string that comes lexicographically first to be less if strings have same size" do
    ("aba" <=> "abc").should == -1
    ("abc" <=> "aba").should == 1
  end

  it "doesn't consider shorter string to be less if longer string starts with shorter one" do
    ("abc" <=> "abcd").should == -1
    ("abcd" <=> "abc").should == 1
  end

  it "compares shorter string with corresponding number of first chars of longer string" do
    ("abx" <=> "abcd").should == 1
    ("abcd" <=> "abx").should == -1
  end
  
  it "ignores subclass differences" do
    a = "hello"
    b = StringSpecs::MyString.new("hello")
    
    (a <=> b).should == 0
    (b <=> a).should == 0
  end
end

# Note: This is inconsistent with Array#<=> which calls #to_ary instead of
# just using it as an indicator.
describe "String#<=>" do
  it "returns nil if its argument does not provide #to_str" do
    ("abc" <=> 1).should == nil
    ("abc" <=> :abc).should == nil
    ("abc" <=> mock('x')).should == nil
  end

  it "returns nil if its argument does not provide #<=>" do
    obj = mock('x')
    ("abc" <=> obj).should == nil
  end

  it "calls #to_str to convert the argument to a String and calls #<=> to compare with self" do
    obj = mock('x')

    # String#<=> merely checks if #to_str is defined on the object. It
    # does not call the method.
    obj.stub!(:to_str)

    obj.should_receive(:<=>).with("abc").and_return(1)

    ("abc" <=> obj).should == -1
  end
end
