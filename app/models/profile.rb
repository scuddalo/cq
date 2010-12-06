class Profile < ActiveRecord::Base
  #include Tidbits::ActiveRecord::ValidatesEmail
  include CellPhone
  include Location::Locatable
  include Friendshipness
  include HasSettings
  include HasPrivacy
  
  belongs_to :user
  
  has_many :favorite_locations, :class_name => 'Location::Favorite'
  has_many :groups, :class_name => 'Group', :foreign_key => 'owner_id'
  #all the seeks that this profile owns
  has_many :seeks, :class_name => 'Seek', :foreign_key => 'owner_id'
  #all the seek requests that this profile sent out
  has_many :seek_requests, :class_name => 'SeekRequest', :foreign_key => 'seeked_profile_id'
  #all the seek responses that this profile sent out.
  has_many :seek_responses, :class_name => 'SeekResponse', :foreign_key => 'responding_profile_id'

  has_attached_file :photo,
    :path => ":rails_root/public/images/:style/:to_param.:extension",
    :url => "/images/:style/:to_param.:extension",
    :styles => {
      :thumb=> ["100x100#"]
    }
  
  #validates_presence_of :email
  #validates_email :message => 'is not a valid email address', :allow_blank => true
  validates_length_of :email, :within => 3..100, :allow_blank => false, :message => 'is not a valid email address'
  #validates_uniqueness_of :email, :case_sensitive => false, :allow_blank => true

  # TODO: remove these and think of a better way to do this
  # ugly ugly hack because I couldn't figure out how to render these attrs 
  # in xml without directly having them in the profile level
  # for more info see location_controller.whos_around and 
  # messages_controller.prepare_messages_for_xml_rendering
  attr_accessor :friends_with_current_profile, :distance, :current_user_tier
  
  def initialize
    @friends_with_current_profile = false
    @distance=-1
  end 
  

  def to_s
    return display_name unless display_name.blank?
    return user.login unless user.blank?
    'Deleted User'
  end
  
  def to_param
    "#{self.id}-#{user.login.to_safe_uri}"
  end

  # doesn't belong here. Todo: think of some other way to do this.
  # see location_controller.whos_around for more info
  def is_friends_with_current_profile
    @friends_with_current_profile
  end
  def distance_from_current_profile
    @distance
  end
  
  def profile
    self
  end
  
  def photo_url
    photo.url(:thumb)
  end
  
  def active_seek_requests
    active_seek_requests = Array.new
    self.seek_requests.each do |seek_request| 
      if seek_request.seek.is_active 
        active_seek_requests << seek_request 
      end
    end
    active_seek_requests
  end
  
  def active_seek
    Seek.find_active_seek(self);
  end
end
