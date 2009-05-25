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
      print "@@@@@@ 1) requested_profile"
      @location = requested_profile.location
    end
    print "@@@@@@ 2) requested_profile"
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
	  debugger
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
        format.xml { render :xml => requested_profile.find_nearby(:include_self=>false).to_xml (:include => [:location, :user])}
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
      @whos_around = requested_profile.find_nearby(:include_self => false)
			respond_to do |format|
	      format.html # new.html.erb
	      format.xml  { render :xml => requested_profile.find_nearby(:include_self => false).to_xml }
	    end
    end
  end
  
end
