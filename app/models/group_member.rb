class GroupMember < ActiveRecord::Base
	belongs_to :group
	belongs_to :friendship
end
