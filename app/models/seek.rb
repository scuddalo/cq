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
end
