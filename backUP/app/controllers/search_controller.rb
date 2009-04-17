 class SearchController < ApplicationController
   def search
      if params[:query]
        @products = Product.search(params[:query])
      else
        @products = []
      end
    end
end


