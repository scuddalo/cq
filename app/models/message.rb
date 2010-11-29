class Message < ActiveRecord::Base
    validates_presence_of :content
    has_one :seek, :class_name => "Seek", :foreign_key => 'message_id'
end
