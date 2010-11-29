xml.messages do
  @messages.each do |msg|
    xml.message do
      xml.from_profile(
        :id => msg.from_profile.id,
        "display-name" => msg.from_profile.display_name
      ) do 
      end
    end  
  end
end