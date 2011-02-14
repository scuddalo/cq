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
        render :xml => requested_profile.active_seek_requests.to_xml(:include => {:seek => {:include => { :owner => {:include => [:user, :location]}, 
                                                                                                          :message => {:only => [:content, :id]},
                                                                                                        }
                                                                                           },
                                                                                  :seeked_profile => {:include => [:user, :location]} 
                                                                                  },
                                                                      :methods => [:is_accepted]
                                                                    )
      }
    end
  end

  def active_seek
    render :xml => requested_profile.active_seek.to_xml({:include => { :owner => {:include => [:user, :location]}, 
                                                                       :message => {:only => [:content, :id]},
                                                                     }
                                                        })
  end
  
  
  def number_of_unreads
    unread_active_seek_req_count = 0
    unread_seek_responses_for_active_seek = 0
    requested_profile.active_seek_requests.each do |req| 
      if !req.message.read
        unread_active_seek_req_count++
      end
    end
    
    requested_profile.active_seek.seek_responses.each do |resp|
      if !resp.message.read
        unread_seek_responses_for_active_seek++
      end
    end
    
    render :xml => {
      :unread_seek_requests => unread_active_seek_req_count,
      :unread_seek_responses => unread_seek_responses_for_active_seek
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
