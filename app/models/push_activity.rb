class PushActivity < ActiveRecord::Base
  def self.last_activity
    res = PushActivity.find(:all, :order => "activity_date desc", :limit => 1)
    res = !res.nil? && res.kind_of?(Array) ? res.first : res;
    res
  end
  
  def self.update_last_activity_time 
    last_one = PushActivity.last_activity
    if(!last_one.nil?) 
      last_one.activity_date = Time.now()
      last_one.save!
  end
end