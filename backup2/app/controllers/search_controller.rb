 class SearchController < ApplicationController
 	def search
	      if params[:query]
	        @users = User.search(params[:query])
	      else
	        @users = []
	      end
		@user_pages, @users = paginate_collection user_collection, :page => params[:page]
	    end
	
	  def paginate_collection(collection, options = {})
	     default_options = {:per_page => 10, :page => 1}
	     options = default_options.merge options
	     
	     pages = Paginator.new self, collection.size, options[:per_page], options[:page]
	     first = pages.current.offset
	     last = [first + options[:per_page], collection.size].min
	     slice = collection[first...last]
	     return [pages, slice]    
	  end 	

end


