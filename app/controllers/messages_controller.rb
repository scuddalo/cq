class MessagesController < ApplicationController
  requires_login
  # GET /messages
  # GET /messages.xml
  def index
    recieved_messages = Message.find(:all, :conditions => {:to_profile_id => current_user.profile.id})
    accepted_messages = Message.find(:all, :conditions => {:from_profile_id => current_user.profile.id, :is_accepted => true})
    @messages = recieved_messages | accepted_messages
    @invitations = Invitation.find(:all, :conditions => {:to_profile_id => current_user.profile.id})

    respond_to do |format|
      format.html # index.html.erb
      format.xml  { 
        prepared_msgs = prepare_messages_for_xml_rendering(@messages)
        render :xml => prepared_msgs.to_xml(
        :include => {
          :from_profile => {
            :include => {
              :user => {}, :location => {} 
            },
            :methods => [:is_friends_with_current_profile, :distance_from_current_profile,:current_user_tier]
          }, 
          :to_profile => {}
        })
      }
    end
  end

  def invitations
    @invitations = Invitation.find(:all, :conditions => {:to_profile_id => current_user.profile.id, :status=>false })
    respond_to do |format|
      format.html # index.html.erb
      # format.xml {render :action => "index.xml.builder", :layout => false}
      format.xml  { 
        render :xml => @invitations.to_xml(
          :include => {
            :from_profile => {
              :include => {
                :user => {}, :location => {}
              },
            },
        })
      }
    end
  end
  
  def prepare_messages_for_xml_rendering(messages)
    prepared_msgs = Array.new
    messages.each do |message|
      message.from_profile.current_user_tier = current_user.profile.which_tier(message.from_profile)
      message.from_profile.distance = current_user.profile.distance_to(message.from_profile)
      is_friend = current_user.profile.friends_with?(message.from_profile)
      message.from_profile.friends_with_current_profile = is_friend
      prepared_msgs << message
    end
    prepared_msgs
  end
 

  def message_sent
    @messages = Message.find(:all, :conditions => {:from_profile_id => current_user.profile.id})

    respond_to do |format|
      format.html # index.html.erb
      format.xml  {
         render :xml => @messages.to_xml(
          :include => {
            :from_profile => {
              :include => [:user, :location],
              :methods => [:is_friends_with_current_profile, :distance_from_current_profile]
              }, 
            :to_profile => {
              :include => [:user, :location],
              :methods => [:is_friends_with_current_profile, :distance_from_current_profile]
            }
          })
      }
    end
  end


  # GET /messages/1
  # GET /messages/1.xml
  def show
    @message = Message.find(params[:id])

    respond_to do |format|
      format.html # show.html.erb
      format.xml  { render :xml => @message }
    end
  end

  # GET /messages/new
  # GET /messages/new.xml
  def new
    @message = Message.new

    respond_to do |format|
      format.html # new.html.erb
      format.xml  { render :xml => @message }
    end
  end

  # POST /messages
  # POST /messages.xml
  def create
    if !params[:profile_ids].nil?
      profile_ids = params[:profile_ids].split(",")
      unless profile_ids.empty?
        profile_ids.each do |id|
          profile = Profile.find_by_id(Integer(id))
          #if its a valid profile, then send a messsage
          if !profile.nil?
            @message = Message.new()
            @message.content=params[:message]
            @message.from_profile_id = current_user.profile.id
            @message.to_profile_id = id
            @message.save!
          end
        end
      end
		elsif params[:group_id] == 'default'
			@group_members = Friendship.find_all_by_follower_id(current_user.id)
			unless @group_members.empty?
				@group_members.each do |i|
					@message = Message.new()
					@message.content=params[:message]
					@message.to_profile_id = i.followee_id
					@message.from_profile_id = current_user.profile.id
					@message.save
				end
			end
		elsif !params[:group_id].nil?
			@group = Group.find(params[:group_id])
	    @group_members = @group.group_members
			unless @group_members.empty?
				@group_members.each do |i|
					@message = Message.new(params[:message])
					@message.to_profile_id = Friendship.find(i.friendship_id).followee_id
					@message.from_profile_id = current_user.profile.id
					@message.save
				end
			end
		else
			@message = Message.new(params[:message])
			@message.from_profile_id = current_user.profile.id
			@message.save
		end		
    
    respond_to do |format|
      format.html {redirect_to messages_path }
      format.xml  { render :xml => @message }
    end  
  end

  
	def send_message

      @message = Message.new
      @message.to_profile_id = params["message_id"]
          
      respond_to do |format|
        format.html # new.html.erb
        format.xml  { render :xml => @message }
      end
	end

  def send_invitation
      @message = Invitation.new
      #@message.content = "<%= link_to 'Accept', follow_path(:profile_id => #{current_user.profile.id}, 
      # :follow_profile_id => #{params[:message_id]}), :method => :post -%> | <%= link_to 'Reject', unfollow_path(:profile_id => 
      #{current_user.profile.id}, :follow_profile_id => #{params[:message_id]}), :method => :post -%>"
      @message.from_profile_id = current_user.profile.id
      @message.to_profile_id = params[:message_id].to_i
      inv = Invitation.find(:first, :conditions => "from_profile_id=#{current_user.profile.id} and to_profile_id=#{params[:message_id].to_i}", :order=> "id asc")
      if inv.nil?
        @message.save
        flash[:notice] = 'Invitation sent successfully.'
      else
        flash[:notice] = 'You have already sent an invitation to this user'
      end
      
      respond_to do |format| 
        format.html {redirect_to whos_around_path(:profile_id => current_user.profile)}
        format.xml {
          render :xml => @message
        }
      end
	end

	def send_message_to_group
    @message = Message.new
		if params["message_id"] == "default"
				@group_message_id = "default"
		else
			@group_message_id = params["message_id"].to_i
		end
		
    respond_to do |format|
      format.html # new.html.erb
      format.xml  { render :xml => @message }
    end
	end


	def show_message
		@message = Message.find(params[:message_id])
		unless @message.from_profile_id == current_user.profile.id
			@message.read = true
			@message.save
		end
	end

	def accept_message
		@message = Message.find(params[:message_id])
		@message.is_accepted = true
		@message.save
        respond_to do |format|
          format.html{
            redirect_to messages_path
          }
          format.xml {
            render :xml => @message
          }
        end
	end

	def reject_message
		@message = Message.find(params[:message_id])
		@message.is_accepted = false
		@message.save
		redirect_to messages_path
	end

    def accept_invitation
      @invitation = Invitation.find(params[:message_id])
      @invitation.status = true
      @invitation.save!
      respond_to do |format|

        format.html {
          redirect_to :controller => :friends, :action => :follow, :profile_id =>@invitation.to_profile_id, :follow_profile_id => @invitation.from_profile_id
        }
        
        format.xml {
          @invitation.from_profile.follow!(@invitation.to_profile)
          render :xml => @invitation.to_profile.followers
        }
      end
    end

    def reject_invitation
      @message = Invitation.find(params[:message_id])
      @message.status = false
      @message.save
      redirect_to messages_path
    end
end
