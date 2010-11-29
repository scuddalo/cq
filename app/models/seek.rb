class Seek < ActiveRecord::Base
  
  belongs_to :owner,
             :class_name => "Profile",
             :foreign_key => "owner_id"

  belongs_to :message,
          :class_name => "Message",
          :foreign_key => "message_id"
          

  has_many :seek_requests

  def self.find_active_seek(seek_owner) 
    Seek.find(:all, :conditions => {:owner_id => seek_owner, :is_active => true})
  end

  def validate_on_create
    self.class.find_active_seek(owner) do |s|
      errors.add(:owner, "already has an active seek") unless s.nil?
    end
  end
end
