class FriendsController < ApplicationController
  
  requires_profile
  requires_login :only => [ :followers ]
  requires_is_self :only => [ :following_by_tier, :follow, :unfollow, :move_to_tier ]
  requires_authorization(Setting::PRIVACY_VIEW_FOLLOWEES, :only => [ :following ])
  append_before_filter :require_follow_profile!, :only => [ :follow, :unfollow, :move_to_tier ]
  append_before_filter :require_requested_tier!, :only => [ :following_by_tier, :move_to_tier ]
  
  helper_method :requested_tier
  

  

  # GET only
  def followers
#@groups = Group.find(:all, :conditions => {:owner_id => params[:profile_id]})
	respond_to do |format|
      format.html # new.html.erb
      format.xml  { render :xml => requested_profile.followers.to_xml }
    end
  end


  # GET only
  # all followings
  def following
	#@friendships = Friendship.find(:all, :conditions => {:follower_id => params[:profile_id]})
	respond_to do |format|
      format.html # new.html.erb
      format.xml  { render :xml => requested_profile.followees.to_xml }
    end
  end
  
  # GET only
  # followings in tier params[:tier]
  def following_by_tier
    raise ActiveRecord::RecordNotFound.new("Parameter tier must be one of [1, 2, 3]") unless (1..3).include?(requested_tier)

	  respond_to do |format|
        format.html # new.html.erb
        format.xml  { render :xml => by_tier = requested_profile.followees.by_tier(requested_tier).to_xml }
    end
  end
  
  # POST only: add a following
  def follow
    requested_profile.follow!(requested_follow_profile)
    redirect_to :action => 'following', :profile_id => current_user.profile, :format => params[:format]
  end
  
  # POST only: delete a following
  def unfollow
    requested_profile.unfollow!(requested_follow_profile)
    redirect_to :action => 'following', :profile_id => current_user.profile
  end
  
  # POST only: moves a following to a different tier
  def move_to_tier
    requested_profile.move_to_tier!(requested_follow_profile, requested_tier)
    redirect_to :action => 'following_by_tier', :profile_id => current_user.profile, :tier => params[:tier]
  end
  
  private
  
  def requested_follow_profile(reload = false)
    safe_load :@follow_profile, Profile, :follow_profile_id, reload
  end
  
  def require_follow_profile!
    require_exists! requested_follow_profile, Profile, :follow_profile_id
  end
  
  def requested_tier
    t = params[:tier]
    @requested_tier = t.blank? ? nil : t.to_i
  end
  
  def require_requested_tier!
    raise ActiveRecord::RecordNotFound.new('Parameter tier is required') unless requested_tier
    true
  end
  
  def handle_invalid_record(exception)
    render :action => :following, :status => 400
  end
  
end
