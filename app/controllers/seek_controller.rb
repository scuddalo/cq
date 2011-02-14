class SeekController < ApplicationController
  requires_login

  # GET /seeks
  # GET /seeks.xml
  def index
    owner = current_user.profile
    seek = Seek.find_active_seek(owner)
    respond_to do |format|
      format.html
      format.xml  { 
        render :xml => seek.to_xml
      }
    end
  end

  #Post
  # /seek/create.xml
  #
  # params expected:
  # ===============
  # seeked_profile_ids
  # message
  def create
    unless current_user.profile.nil?
      owner = current_user.profile
      #handle existing active seeks first
      Seek.find_active_seek(owner).each do |s|
        s.is_active = false;
        s.save!
      end

      #
      #now create the new seek
      @seek = Seek.new(:owner => owner) do |seek|
        seeked_profile_ids = params[:seeked_profile_ids].split(",")
        #seek requests
        seeked_profile_ids.each do |seeked_profile_id|
          seeked_profile = Profile.find_by_id(seeked_profile_id)
          #seek_msg
          seek_message = Message.new(:content => params[:message])
          seek.seek_requests << SeekRequest.new(:seek => seek, :seeked_profile => seeked_profile, :message => seek_message)
        end

        #is_active
        seek.is_active = true
        seek.save!
      end
      respond_to do |format|
        format.html 
        format.xml  { 
          render :xml => @seek.to_xml(:include=>{ :owner => {:include =>[:user,:location] }, 
                                                  :seek_requests=>{ :include => {
                                                                                  :seeked_profile => {:include => [:user, :location]},
                                                                                  :message => {}
                                                                                }
                                                                  }
                                                }
                                     )
        }
      end

    end
  end

  #GET
  #seek/requests.xml
  # returns all active_seek_requests for the current_user
  def seek_requests 
    @seek_requests = current_user.profile.active_seek_requests
    respond_to do |format|
      format.html
      format.xml {
        render :xml => @seek_requests.to_xml(:include => {:seek => {:include => {:owner => {:include => [:user, :location]}, 
                                                                                 
                                                                                }
                                                                   },
                                                          :seeked_profile => {:include => [:user, :location]}, 
                                                          :message => {:only => [:content, :read, :id]},
                                                         },
                                              :methods => [:is_accepted]
                                            )
      }
    end
  end
  
  # GET
  # /seek/:seek_id/requests.xml
  # returns all seek_requests related to the seek_id
  def seek_requests_for_seek
    if !params[:seek_id].nil?
      seek = Seek.find_by_id(params[:seek_id])
      # make sure that the owner of the seek is the one 
      # asking for seek_requests
      if (seek.owner.profile.id == current_user.profile.id)
        @seek_requests = seek.seek_requests
      else 
        # if not send back an empty response
        @seek_requests = []
      end
      respond_to do |format|
        format.html
        format.xml {
          render :xml => @seek_requests.to_xml(:include => {:seek => {:include => {:owner => {:include => [:user, :location]}, 
                                                                                  }
                                                                     },
                                                            :seeked_profile => {:include => [:user, :location]},
                                                            :message => {:only => [:content, :id]}
                                                           },
                                                :methods => [:is_accepted]
                                                           
                                              )
        }
      end
    end
  end

  # POST
  # /seek/response/accept.xml
  # params => {:seek_request_ids, :message}
  def accept_seek
    seek_request_ids = params[:seek_request_ids]
    @seek_responses = create_seek_response(seek_request_ids, params[:message],  true)
    #render
    respond_to do |format|
      format.html
      format.xml {
        render :xml => @seek_responses.to_xml
      }
    end
  end
  
  # POST
  # /seek/request/:seek_request_id/mark_read.xml
  def mark_seek_request_as_read
    seek_request_id = params[:seek_request_id]
    seek_request = SeekRequest.find_by_id(seek_request_id)
    seek_request.message.read = 1;
    seek_request.message.save!
    respond_to do |format| 
       format.html 
       format.xml { head :ok }  
    end
  end
  

  # POST
  # /seek/response/reject.xml
  # params => {:seek_request_ids, :message}
  def reject_seek
    seek_request_ids = params[:seek_request_ids]
    @seek_responses = create_seek_response(:seek_request_ids => seek_request_ids, :message => params[:message], :accept => false)
    #render
    respond_to do |format|
      format.html
      format.xml {
        render :xml => @seek_responses.to_xml
      }
    end
  end
  
  def number_of_seeks
    
  end

  
  def create_seek_response(seek_request_ids, message, accept)
    @seek_responses = Array.new
      for sid in seek_request_ids.split(",") do 
        seek_request = SeekRequest.find_by_id(sid)
        unless seek_request.nil?
          debugger
          seek_response = SeekResponse.new(:responding_profile => current_user.profile, :seek => seek_request.seek, :accept => accept)
          #message
          msg = Message.new(:content => message)
          seek_response.message = msg
          seek_response.save!
          @seek_responses << seek_response
        end
      end
    @seek_responses
  end

  
end
