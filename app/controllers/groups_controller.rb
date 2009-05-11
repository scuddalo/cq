class GroupsController < ApplicationController
  # GET /groups
  # GET /groups.xml
  requires_login

  def index
 
    @groups = Group.find(:all, :conditions => {:owner_id => current_user.profile.id})

    respond_to do |format|
      format.html # index.html.erb
      format.xml  { render :xml => @groups }
    end
  end

  # GET /groups/1
  # GET /groups/1.xml
  def show
    @group = Group.find(params[:group_id])

    respond_to do |format|
      format.html # show.html.erb
      format.xml  { render :xml => @group }
    end
  end

  # GET /groups/new
  # GET /groups/new.xml
  def new
    @group = Group.new

    respond_to do |format|
      format.html # new.html.erb
      format.xml  { render :xml => @group }
    end
  end

  # GET /groups/1/edit
  def edit
    @group = Group.find(params[:group_id])
  end

  # POST /groups
  # POST /groups.xml
  def create
    @group = Group.new(params[:group])
		@group.owner_id = current_user.profile.id

    respond_to do |format|
      if @group.save
        flash[:notice] = 'Group was successfully created.'
        format.html { redirect_to(@group) }
        format.xml  { render :xml => @group, :status => :created, :location => @group }
      else
        format.html { render :action => "new" }
        format.xml  { render :xml => @group.errors, :status => :unprocessable_entity }
      end
    end
  end

  # PUT /groups/1
  # PUT /groups/1.xml
  def update
    @group = Group.find(params[:group_id])

    respond_to do |format|
      if @group.update_attributes(params[:group])
        flash[:notice] = 'Group was successfully updated.'
        format.html { redirect_to(@group) }
        format.xml  { head :ok }
      else
        format.html { render :action => "edit" }
        format.xml  { render :xml => @group.errors, :status => :unprocessable_entity }
      end
    end
  end

  # DELETE /groups/1
  # DELETE /groups/1.xml
  def destroy
    @group = Group.find(params[:group_id])
    @group.destroy

    respond_to do |format|
      format.html { redirect_to(groups_url) }
      format.xml  { head :ok }
    end
  end

  def group_members
		if params[:group_id] == 'default'
			@group_members = Friendship.find_all_by_follower_id(current_user.id)
		## Added by Dev1	
		respond_to do |format|
      format.html # new.html.erb
      format.xml  { render :xml => Friendship.find_all_by_follower_id(current_user.id) }
    end
		else
	    @group = Group.find(params[:group_id])
			@group_members = @group.group_members
		end
  end

	def add_friends_in_group
		@group = Group.find(params[:group_id])
		@group_members = Friendship.find_all_by_follower_id(current_user.id)
	end

	def create_group_members
		@group = Group.find(params[:group_id])
    unless params["friendship"]["friendship_ids"].empty?
			params["friendship"]["friendship_ids"].each do |i|
				GroupMember.create(:group_id => params[:group_id], :friendship_id => i)
			end
		redirect_to :action => :group_members, :id => @group
		end
		
		@group_members = Friendship.find_all_by_follower_id(current_user.id)
	end

	def remove_friends_in_group
		@group = Group.find(params[:group_id])
		GroupMember.destroy(params[:member_id])
		redirect_to :action => :group_members, :id => @group
	end

end
