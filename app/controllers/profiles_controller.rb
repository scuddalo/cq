class ProfilesController < ApplicationController
  
  requires_profile
  requires_authorization(Setting::PRIVACY_VIEW_PROFILE, :only => [ :show ])
  requires_is_self :except => [ :show ]
  
  # GET only
  def dashboard
  end
  
  # GET only
  def getting_started
  end

  # GET only
  def show
  end

  # GET only
  def edit
  end



  # POST only
  def update
    update_profile(params[:profile])
    update_password(params[:old_password], params[:new_password], params[:new_password_confirmation])
    redirect_to :action => 'show'
  end
  
  def upload_photo
    requested_profile.photo = params["profile"]["photo"]
    requested_profile.save!
    respond_to do |format|
      format.xml { 
                    render :xml => requested_profile.to_xml(:include => [:location, :user], :methods => [:is_friends_with_current_profile, :distance_from_current_profile, :photo_url, :current_user_tier])
                 }
    end
  end
  
  def update_status
    if !params[:status].nil?
      requested_profile.status = params[:status]
      requested_profile.save!  
       respond_to do |format|
          format.xml { 
                        render :xml => requested_profile.to_xml(:include => [:location, :user], :methods => [:is_friends_with_current_profile, :distance_from_current_profile, :photo_url, :current_user_tier])
                     }
        end
    end
  end
  
  def seek_requests
    respond_to do |format|
      format.xml {
      #   seek_requests_xml = ""
      #   builder = Builder::XmlMarkup.new(:target=>seek_requests_xml, :indent=>2)
      #   requested_profile.active_seek_requests.each do |seek_request|
      #     builder.
      #     
      #   end
        render :xml => requested_profile.active_seek_requests.to_xml(:include => {:seek => {:include => { :owner => {:include => [:user, :location]}
                                                                                                        }
                                                                                           },
                                                                                  :seeked_profile => {:include => [:user, :location]}, 
                                                                                  :message => {:only => [:content, :id, :read]},
                                                                                  },
                                                                      :methods => [:is_accepted]
                                                                    )
      }
    end
  end

  def active_seek
    render :xml => requested_profile.active_seek.to_xml({:include => { :owner => {:include => [:user, :location]}, 
                                                                       :seek_requests => {:include => [:message]}
                                                                     }
                                                        })
  end
  
  
  def number_of_unreads
    ignore_last_activity_time = params[:ignoreLastActivity]
    puts "###############ignoreLastActivity: #{ignore_last_activity_time}"
    # first fetch all the seek requests 
    all_seek_requests = requested_profile.active_seek_requests_since_last_push((ignore_last_activity_time == "1"))
    seek_req_count = all_seek_requests.count
    
    unread_seek_requests = requested_profile.active_seek_requests_with_read_status(false)
    
    # second, fetch all seek responses
    active_seek = requested_profile.active_seek
    all_seek_responses = active_seek.nil? ? Array.new : active_seek.seek_responses_since_last_activity((ignore_last_activity_time == "1"))
    seek_res_count = all_seek_responses.nil? ? 0 : all_seek_responses.count
    
    PushActivity.update_last_activity_time()
    
    render :xml => {
      :new_seek_requests => seek_req_count,
      :new_seek_responses => seek_res_count, 
      :unread_seek_requests => unread_seek_requests.count
    }.to_xml

  end
  
  # GET only
  def refer_a_friend_form
    render :action => 'refer_a_friend'
  end
  
  # POST only
  def refer_a_friend
    redirect_to :profile_id => current_user, :action => 'dashboard'
  end
  
  private
  
  def update_profile(profile_hash)
    if profile_hash && !profile_hash.empty?
      requested_profile.update_attributes!(profile_hash)
      current_user.profile.reload # current_user.profile is a different reference to the same object (unless admin?)
      flash[:notice] = 'Your profile has been updated'
    end
  end
  
  def update_password(op, np, npc)
    if op || np || npc
      requested_profile.user.change_password!(op, np, npc)
      flash['notice 2'] = 'Your password has been changed'
    end
  end
  

  
end
