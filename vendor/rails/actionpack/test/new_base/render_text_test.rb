require File.join(File.expand_path(File.dirname(__FILE__)), "test_helper")

class ApplicationController < ActionController::Base
end

module RenderText
  class SimpleController < ActionController::Base
    self.view_paths = [ActionView::Template::FixturePath.new]
    
    def index
      render :text => "hello david"
    end
  end
  
  class TestSimpleTextRenderWithNoLayout < SimpleRouteCase
    describe "Rendering text from a action with default options renders the text with the layout"
    
    get "/render_text/simple"
    assert_body   "hello david"
    assert_status 200
  end
  
  class WithLayoutController < ::ApplicationController
    self.view_paths = [ActionView::Template::FixturePath.new(
      "layouts/application.html.erb" => "<%= yield %>, I'm here!",
      "layouts/greetings.html.erb"   => "<%= yield %>, I wish thee well.",
      "layouts/ivar.html.erb"        => "<%= yield %>, <%= @ivar %>"
    )]    
    
    def index
      render :text => "hello david"
    end
    
    def custom_code
      render :text => "hello world", :status => 404
    end
    
    def with_custom_code_as_string
      render :text => "hello world", :status => "404 Not Found"
    end
    
    def with_nil
      render :text => nil
    end
    
    def with_nil_and_status
      render :text => nil, :status => 403
    end

    def with_false
      render :text => false
    end
    
    def with_layout_true
      render :text => "hello world", :layout => true
    end
    
    def with_layout_false
      render :text => "hello world", :layout => false
    end
    
    def with_layout_nil
      render :text => "hello world", :layout => nil
    end
    
    def with_custom_layout
      render :text => "hello world", :layout => "greetings"
    end
    
    def with_ivar_in_layout
      @ivar = "hello world"
      render :text => "hello world", :layout => "ivar"
    end
  end

  class TestSimpleTextRenderWithLayout < SimpleRouteCase    
    describe "Rendering text from a action with default options renders the text without the layout"
    
    get "/render_text/with_layout"
    assert_body   "hello david"
    assert_status 200
  end
  
  class TestTextRenderWithStatus < SimpleRouteCase
    describe "Rendering text, while also providing a custom status code"
    
    get "/render_text/with_layout/custom_code"
    assert_body   "hello world"
    assert_status 404
  end
  
  class TestTextRenderWithNil < SimpleRouteCase
    describe "Rendering text with nil returns a single space character"
    
    get "/render_text/with_layout/with_nil"
    assert_body   " "
    assert_status 200
  end
  
  class TestTextRenderWithNilAndStatus < SimpleRouteCase
    describe "Rendering text with nil and custom status code returns a single space character with the status"
    
    get "/render_text/with_layout/with_nil_and_status"
    assert_body   " "
    assert_status 403
  end
  
  class TestTextRenderWithFalse < SimpleRouteCase
    describe "Rendering text with false returns the string 'false'"
    
    get "/render_text/with_layout/with_false"
    assert_body   "false"
    assert_status 200
  end
  
  class TestTextRenderWithLayoutTrue < SimpleRouteCase
    describe "Rendering text with :layout => true"
    
    get "/render_text/with_layout/with_layout_true"
    assert_body "hello world, I'm here!"
    assert_status 200
  end
  
  class TestTextRenderWithCustomLayout < SimpleRouteCase
    describe "Rendering text with :layout => 'greetings'"
    
    get "/render_text/with_layout/with_custom_layout"
    assert_body "hello world, I wish thee well."
    assert_status 200
  end
  
  class TestTextRenderWithLayoutFalse < SimpleRouteCase
    describe "Rendering text with :layout => false"
    
    get "/render_text/with_layout/with_layout_false"
    assert_body "hello world"
    assert_status 200
  end
  
  class TestTextRenderWithLayoutNil < SimpleRouteCase
    describe "Rendering text with :layout => nil"
    
    get "/render_text/with_layout/with_layout_nil"
    assert_body "hello world"
    assert_status 200
  end
end

ActionController::Base.app_loaded!