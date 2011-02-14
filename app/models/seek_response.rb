class SeekResponse < ActiveRecord::Base
  validates_presence_of :seek,:responding_profile
  belongs_to :seek,
             :class_name => "Seek",
             :foreign_key => "seek_id"
             
  belongs_to :responding_profile,
             :class_name => "Profile",
             :foreign_key => "responding_profile_id"
  has_one :message


  def self.seek_response_for_a_seek(prof) 
    SeekResponse.find(:all, :conditions => {:seek_id => prof.active_seek.id});
  end
end
