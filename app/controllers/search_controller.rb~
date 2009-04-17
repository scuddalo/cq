class SearchController < ApplicationController

	def index
		logger.info ("***************** I am in search_controller.search")
	end
	
	def find_profile
	logger.info ("***************** I am in search_controller.find_profile")
  	end

	def result
	logger.info ("***************** I am in search_controller.result")
	   #@user = User.find(:all, :conditions => { :login => params[:search_profile] })
	@results = User.find(:all, :conditions => ['login LIKE ?', params[:search_profile]+'%' ])

  	end
	
end

