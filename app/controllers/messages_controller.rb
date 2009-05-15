class MessagesController < ApplicationController
  # GET /messages
  # GET /messages.xml
  def index
    @messages = Message.find(:all, :conditions => {:to_profile_id => current_user.profile.id})
    @invitations = Invitation.find(:all, :conditions => {:to_profile_id => current_user.profile.id})

    respond_to do |format|
      format.html # index.html.erb
      format.xml  { render :xml => @messages }
    end
  end

	def message_sent

    @messages = Message.find(:all, :conditions => {:from_profile_id => current_user.profile.id})

    respond_to do |format|
      format.html # index.html.erb
      format.xml  { render :xml => @messages }
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

		if params[:group_id] == 'default'
			@group_members = Friendship.find_all_by_follower_id(current_user.id)
			unless @group_members.empty?
				@group_members.each do |i|
					@message = Message.new(params[:message])
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
      
    flash[:notice] = 'Message sent successfully.'
    redirect_to messages_path      
    
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
      #@message.content = "<%= link_to 'Accept', follow_path(:profile_id => #{current_user.profile.id}, :follow_profile_id => #{params[:message_id]}), :method => :post -%> | <%= link_to 'Reject', unfollow_path(:profile_id => #{current_user.profile.id}, :follow_profile_id => #{params[:message_id]}), :method => :post -%>"
			@message.from_profile_id = current_user.profile.id
      @message.to_profile_id = params[:message_id]
			@message.save
     flash[:notice] = 'Invitation sent successfully.'
    redirect_to whos_around_path(:profile_id => current_user.profile)
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
		redirect_to messages_path
	end

	def reject_message
		@message = Message.find(params[:message_id])
		@message.is_accepted = false
		@message.save
		redirect_to messages_path
	end

end
