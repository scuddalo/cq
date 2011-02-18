class PushActivity < ActiveRecord::Base
  attr_accessor :activity_date
  
  def last_activity_date 
    PushActivity.find(:all, :order => "activity_date desc", :limit => 1)
  end
end