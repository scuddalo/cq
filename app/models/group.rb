class Group < ActiveRecord::Base
	belongs_to :profile, :class_name => 'Profile', :foreign_key => 'owner_id'
	has_many :group_members
end
