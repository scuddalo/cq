class SeekResponse < ActiveRecord::Base
  validates_presence_of :seek,:responding_profile
  belongs_to :seek,
             :class_name => "Seek",
             :foreign_key => "seek_id"
  belongs_to :responding_profile,
             :class_name => "Profile",
             :foreign_key => "responding_profile_id"
  has_one :message,
          :class_name => "Message",
          :foreign_key => "message_id"
end