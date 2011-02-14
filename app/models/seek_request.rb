class SeekRequest < ActiveRecord::Base
  validates_presence_of :seeked_profile, :seek
  belongs_to :seek,
             :class_name => "Seek",
             :foreign_key => "seek_id"
  belongs_to :seeked_profile,
             :class_name => "Profile",
             :foreign_key => "seeked_profile_id"
  
  belongs_to :message,
             :class_name => "Message",
             :foreign_key => "message_id"
  
  def is_accepted
    seek_response = SeekResponse.find(:first, :conditions=>["seek_id = ? and responding_profile_id = ?", self.seek, self.seeked_profile], :order => "id desc")
    if seek_response.nil?
      false
    else
      seek_response.accept
    end
  end
  
  def mark_as_read
    messsase.read = 1;
    message.save!
  end
end
