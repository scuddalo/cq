ActionController::Routing::Routes.draw do |map|

  # The following methods can be used in block form, like so:
  #   map.html_or_api do |m|
  #     m.connect ...
  #   end
  # or to generate a defalt hash of options, like so:
  #   map.connect '...', html_get(:action => '...')
  #
  # * get
  # * post
  # * html
  # * html_or_api
  # * html_or_api_or_feeds
  # * html_get              (great for forms that can only be seen in browser)
  # * html_or_api_get       (great for data that can be seen in browser or via the API)
  # * html_or_api_post      (great for data to be updated in browser or via API)

  map.resources :messages,:member => {
    :send_message => :get,
    :send_invitation => :post,
    :invitations => :get,
    :send_message_to_group => :get, 
    :show_message => :post,
    :accept_message => :post,
    :reject_message => :post, 
    :accept_invitation => :post, 
    :reject_invitation => :post},
:collection => {:message_sent=> :get}

  #map.connect "messages/:message_id/invitations.:format", :controller => "messages", :action => "invitations"


  map.resources :groups ,:member => {:group_members => :get,:add_friends_in_group => :get,:create_group_members => :post,:remove_friends_in_group => :get}

  map.root :controller => 'static', :action => 'welcome'
  
  map.get(:controller => 'static') do |m|
    m.help_index  'docs.:format', :action => 'index'
    m.document    'docs/:page.:format', :action => 'page'
  end



  map.connect "search/",:controller => "search",:action     => "index"
  map.connect "search/result",:controller => "search",:action     => "result"
  map.connect "friends/follow",:controller => "friends",:action     => "follow"
  map.connect "group/",:controller => "group",:action     => "index"


  map.with_options(:controller => 'seek') do |m|
    m.post do |n| 
      n.create_seek 'seek/create.:format', :action => 'create'
      n.accept_seek 'seek/response/accept.:format', :action => 'accept_seek'
      n.reject_seek 'seek/response/reject.:format', :action => 'reject_seek'
      n.mark_as_read '/seek/request/:seek_request_id/mark_read', :action => 'mark_seek_request_as_read'
    end

    m.get do |n|
      n.seek_requests 'seek/requests.:format', :action => 'seek_requests'
      n.seek_requests 'seek/:seek_id/requests.:format', :action => 'seek_requests_for_seek'
    end
  end



  map.with_options(:controller => 'accounts') do |m|
    m.get do |n|
      n.login_form 'accounts/login.:format', :action => 'login'
      n.signup_form 'accounts/signup.:format', :action => 'signup'
      
      # TODO: should logout be POST only?  If so, how do we fake POST through GET?
      n.logout 'accounts/logout.:format', :action => 'logout'
    end
    
    m.post do |n|
      n.password_login 'accounts/login/password.:format', :action => 'password_login'
      n.password_signup 'accounts/signup/password.:format', :action => 'password_signup'
    end
  end
  
  map.with_options(:controller => 'profiles') do |m|
    m.get do |n|
      n.profile 'people/:profile_id.:format', :action => 'show'
      n.dashboard 'people/:profile_id/dashboard.:format', :action => 'dashboard'
      n.getting_started 'people/:profile_id/getting_started.:format', :action => 'getting_started'
      n.edit_profile 'people/:profile_id/edit.:format', :action => 'edit'
      n.refer_a_friend 'refer_a_friend.:format', :action => 'refer_a_friend_form'
      n.active_seek_requests 'people/:profile_id/seek_requests.:format', :action => 'seek_requests'
      n.active_seek 'people/:profile_id/active_seek.:format', :action => 'active_seek'
      ##n.profile_search 'people/search.:format', :action => 'search'
      ##n.profile_search_results 'people/search/results.:format', :action => 'find_profile'
    end
    
    m.post do |n|
      n.connect 'people/:profile_id.:format', :action => 'update'
      n.connect 'people/:profile_id/upload_photo.:format', :action => 'upload_photo'
      n.connect 'people/:profile_id/update_status.:format', :action => 'update_status'
    end
    
    m.post do |n|
      n.connect 'refer_a_friend.:format', :action => 'refer_a_friend'
    end
  end
  
  map.with_options(:controller => 'friends') do |m|
    m.get do |n|
      n.following 'people/:profile_id/following.:format', :action => 'following'
      n.following_by_tier 'people/:profile_id/following/tier/:tier.:format', :action => 'following_by_tier'
      n.followers 'people/:profile_id/followers.:format', :action => 'followers'
    end
    m.post do |n|
      n.follow 'people/:profile_id/follow.:format', :action => 'follow'
			#n.connect 'friends.:format', :action => 'follow'
      n.unfollow 'people/:profile_id/unfollow.:format', :action => 'unfollow'
      n.move_to_tier 'people/:profile_id/move_to_tier.:format', :action => 'move_to_tier'
    end
  end

  map.with_options(:controller => 'location') do |m|
    m.get do |n|
      n.location 'people/:profile_id/location.:format', :action => 'current'
      n.whos_around 'people/:profile_id/location/whos_around.:format', :action => 'whos_around'
      n.edit_location 'people/:profile_id/location/edit.:format', :action => 'edit'
    end
    
    m.post do |n|
      n.connect 'location.:format', :action => 'update'
      n.connect 'location/location_modify.:format', :action => 'modify'
    end


    m.put do |n|
      n.connect 'people/:profile_id/location.:format', :action => 'update'
    end
  end
  
  map.resources :favorite_locations,
                :path_prefix => '/people/:profile_id/location',
                :as => 'favorites',
                :member => { :set_current => :post }
  
  map.with_options(:controller => 'settings') do |m|
    m.get do |n|
      n.edit_notification_settings 'people/:profile_id/settings/notifications.:format', :action => 'edit_notifications'
      n.edit_privacy_settings 'people/:profile_id/settings/privacy.:format', :action => 'edit_privacy'
    end
    
    m.post do |n|
      n.update_notification_settings 'people/:profile_id/settings/notifications.:format', :action => 'update_notifications'
      n.update_privacy_settings 'people/:profile_id/settings/privacy.:format', :action => 'update_privacy'
    end
  end
  
  map.get(:controller => 'css') do |m|
    # generated stylesheets (can't do a generic route, since it would hide /public/stylesheets)
    m.connect '/stylesheets/color.css', :action => 'color', :format => 'css'
    m.connect '/stylesheets/browser.css', :action => 'browser', :format => 'css'
  end
end
