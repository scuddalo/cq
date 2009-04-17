# A mixin providing the methods for friendshipness.
# Mixing-in class +must+ provide a +profile+ method that
# returns a Profile.  (User and Profile both do.)
module Friendshipness
  
  module ByTier
    def by_tier(tier)
      find(:all, :conditions => ["friendships.tier = ?", tier])
    end
  end
  
  def self.included(base)
    base.has_many :follower_friends, :class_name => 'Friendship', :foreign_key => 'follower_id'
    base.has_many :followee_friends, :class_name => 'Friendship', :foreign_key => 'followee_id'
    base.has_many :followers, :through => :followee_friends, :source => :follower, :extend => Friendshipness::ByTier
    base.has_many :followees, :through => :follower_friends, :source => :followee, :extend => Friendshipness::ByTier
  end
  
  # Friend Methods
  
  def friendships(force_reload = false)
    @friendships = nil if force_reload
    @friendships ||= profile.follower_friends(force_reload).mutual
  end
  
  def friends(force_reload = false)
    @friends = nil if force_reload
    @friends ||= friendships(force_reload).map { |f| f.followee }
  end
  
  def friends_with?(user_or_profile)
    profile.friends.include?(user_or_profile.profile)
  end
  
  def followed_by?(user_or_profile)
    profile.followers.include?(user_or_profile.profile)
  end
  
  def following?(user_or_profile)
    profile.followees.include?(user_or_profile.profile)
  end
  
  def following_in_tiers?(user_or_profile, *tiers)
    result = false
    Friendship.find_friendship(self, user_or_profile.profile) do |f|
      result = (f && tiers.include?(f.tier))
    end
    result
  end
  
  def follow!(user_or_profile, tier = 3)
    f = Friendship.new(:follower => self.profile, :followee => user_or_profile.profile, :tier => tier)
    f.save!
    profile.follower_friends(true)
    f
  end
  
  def unfollow!(user_or_profile)
    find_following!(user_or_profile) do |f|
      f.destroy
    end  
    profile.follower_friends(true)
    nil
  end
  
  def move_to_tier!(user_or_profile, new_tier)
    f = find_following!(user_or_profile) do |f|
      if f.tier == new_tier
        errors.add(:base, "#{user_or_profile} is already in tier #{new_tier}")
        raise ActiveRecord::RecordInvalid.new(self)
      else
        f.tier = new_tier
        f.save!
        f
      end 
    end
    profile.follower_friends(true)
    f
  end
  
  private
  
  def find_following!(user_or_profile, &block)
    result = nil
    transaction do
      Friendship.find_friendship!(self, user_or_profile.profile) do |f|
        result = block.call(f)
      end
    end
    result
  end
  
end