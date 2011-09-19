class LocationController < ApplicationController

  requires_profile :only => [ :current ]
  requires_is_self :except => [ :current ]
	# The default radius for searches
  define_once :DEFAULT_DISTANCE, 50
  
  # GET only
  def current
    @location = requested_profile.location || 'Nowhere'
  end
  
  # GET only
  def edit
    if !requested_profile
      @location = requested_profile.location
    end
  end

  # POST only
  def update
    if params[:lat_and_long] == "1"
      requested_profile.location = Location::parse(params[:location], [], true)
    else
      requested_profile.location = params[:location]
    end
    requested_profile.location.save!
    requested_profile.save!
    current_user.reload
    flash[:notice] = "Updated your location to #{requested_profile.location}"
    if requested_profile.location
      redirect_to :action => 'whos_around'
    else
      redirect_to :controller => 'profiles', :action => 'dashboard', :profile_id => current_user
    end
  end


	def modify
	  if params[:location]
      requested_profile.location = params[:location]
    elsif params[:lat] and params[:long]
      requested_profile.location = Location::parse("#{params[:lat]},#{params[:long]}", [], true)
  	else
    end
    requested_profile.location.save!
    requested_profile.save!
		
    current_user.reload
    flash[:notice] = "Updated your location to #{requested_profile.location}"
    
		if requested_profile.location
      respond_to do |format|
        format.html {redirect_to :action => 'whos_around'}
        format.xml { 
          @whos_around = requested_profile.find_nearby(:include_self=>false)
          final_profiles = distinguish_friends_and_strangers(@whos_around)
          final_profiles.each { |p| p.current_user_tier = requested_profile.which_tier(p) }
          final_profiles = final_profiles.select {|p| requested_profile.friends_with?(p) }
          render :xml => final_profiles.to_xml(:include => [:location, :user], :methods => [:is_friends_with_current_profile, :distance_from_current_profile, :current_user_tier])
        }
          #render :xml => requested_profile.find_nearby(:include_self=>false).to_xml (:include => [:location, :user], :methods => [:is_friends_with_current_profile])}
      end
    else
      redirect_to :controller => 'profiles', :action => 'dashboard', :profile_id => current_user
    end
  end



  # GET only
  def whos_around
    if requested_profile.location.nil?
      flash[:notice] = "Please set your current location to see who's around"
      redirect_to :action => 'edit'
    else
      @whos_around = requested_profile.find_nearby( { :include_self => false, :tier => params[:tier], :distance => 2700 })
      puts @whos_arround
      respond_to do |format|
        format.html # new.html.erb
        format.xml  { 
          final_xml = ""
          final_profiles = distinguish_friends_and_strangers(@whos_around)
          final_profiles.each { |p| p.current_user_tier = requested_profile.which_tier(p) }
          final_profiles = final_profiles.select {|p| requested_profile.following?(p) }
          final_profiles = final_profiles.select {|a| !a.offline}
          render :xml => final_profiles.to_xml(:include => [:location, :user], :methods => [:is_friends_with_current_profile, :distance_from_current_profile, :current_user_tier])
          #render :xml => @whos_around.to_xml(:include => [:location, :user]) 
        }
      end
    end
  end
  
  def distinguish_friends_and_strangers(profiles=[])
      final_profiles = Array.new
      profiles.each do |prof|
        prof.friends(true)
        is_friend = requested_profile.friends_with?(prof)
        logger.info("@@@@ #{is_friend}")

        prof.friends_with_current_profile = is_friend
        prof.distance = prof.distance_to(requested_profile)
        #prof.current_user_tier = -1
        final_profiles << prof
      end
      final_profiles
  end
  
end
