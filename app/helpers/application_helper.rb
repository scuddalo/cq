# Methods added to this helper will be available to all templates in the application.
module ApplicationHelper
  include Avatar::View::AbstractViewSupport
  include Forms
  
  private
  
  def title(page_title)
    content_for(:title, page_title || 'Default Title')
  end
  
  def avatar_tag(user_or_profile, avatar_options={}, html_options={})
    return '' if user_or_profile.nil?
    avatar_options = {:size => 40}.merge(avatar_options)
    html_options = {:alt => '(Avatar)'}.merge(html_options)
    url = avatar_url_for(user_or_profile.profile, avatar_options)
    image_tag(url, html_options)
  end
  
  def link_to_profile(user_or_profile, options = {}, &block)
    profile = user_or_profile.profile
    text = profile.to_s
    options = {
      :avatar => true, :text => true, :capitalize => true
    }.merge(options)
    text = display_name(profile, options)
    options[:title] ||= text
    content = ''
    content << avatar_tag(profile, options, options.pass(:alt)) if options[:avatar]
    content << '&nbsp;' if options[:avatar] && options[:text]
    content << text if options[:text]
    content << capture(&block) if block_given?
    link_to(content, profile_path(profile), :title => options[:title])
  end
  
  def nav_section(s = nil)
    if s
      clear_content_for(:nav_section)
      content_for(:nav_section, s)
    end
  end
  
  def no_content_for(symbol)
    if symbol
      clear_content_for(symbol)
    end
  end
  
  def render_flash(message = 'There were some problems with your submission:')
    if flash[:notice]
      flash_to_display, level = flash[:notice], 'notice'
    elsif flash[:warning]
      flash_to_display, level = flash[:warning], 'warning'
    elsif flash[:error]
      level = 'error'
      if flash[:error].instance_of?(ActiveRecord::Errors) || flash[:error].is_a?(Hash)
        flash_to_display = message
        flash_to_display << activerecord_error_list(flash[:error])
      else
        flash_to_display = flash[:error]
      end
    else
      return
    end
    content_tag 'div', flash_to_display, :class => "flash #{level}"
  end
  
  def clear_content_for(symbol)
    instance_variable_set("@content_for_#{symbol}", '')
  end

  def activerecord_error_list(errors)
    error_list = '<ul class="error_list">'
    error_list << errors.collect do |e, m|
      "<li>#{e.humanize unless e == "base"} #{m}</li>"
    end.to_s << '</ul>'
    error_list
  end
  
  
  # Exactly as defined in vendor/plugins/grammar/lib/grammar/ext/action_view,
  # but without the additional binding that is no longer needed.
  #
  # Avoids deprecation warning.
  def with_grammatical_context(options_or_context = nil, &block) #:yields context
    context = case options_or_context
    when nil
      self.grammatical_context
    when Hash, Grammar::GrammaticalContext
      self.grammatical_context.merge(options_or_context.to_hash)
    else
      raise "I don't know how to use #{options_or_context} as a GrammaticalContext"
    end
    
    concat(capture(context, &block))
  end
  
end
