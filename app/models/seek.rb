class Seek < ActiveRecord::Base  
  belongs_to :owner,
             :class_name => "Profile",
             :foreign_key => "owner_id"


  has_many :seek_requests
  
  has_many :seek_responses, 
           :class_name => "SeekResponse", 
           :foreign_key => "seek_id"

  def self.find_active_seek(seek_owner) 
    Seek.find(:all, :conditions => {:owner_id => seek_owner, :is_active => true})
  end

  def validate_on_create
    self.class.find_active_seek(owner) do |s|
      errors.add(:owner, "already has an active seek") unless s.nil?
    end
  end
  
  def seek_responses_since_last_activity(ignore_last_activity_time)
    if !ignore_last_activity_time
      last_activity = PushActivity.find(:all, :order => "activity_date desc", :limit=> 1)
      last_activity = last_activity.kind_of?(Array) ? last_activity.first : last_activity
      seek_responses = SeekResponse.find(:all, 
                        :joins => "join seeks on seeks.id = seek_responses.seek_id",
                        :conditions => [
                                         "seek_responses.seek_id = ? and seek_responses.updated_at > ?", 
                                         self.id, 
                                         last_activity.activity_date.to_s(:db)
                                      ]
                       )

    else
      seek_responses = SeekResponse.find(:all, 
                        :joins => "join seeks on seeks.id = seek_responses.seek_id",
                        :conditions => [
                                         "seek_responses.seek_id = ? ", 
                                         self.id
                                      ]
                       )
        
    end
    result = seek_responses.nil? ? Array.new : seek_responses
    result
  end
end
