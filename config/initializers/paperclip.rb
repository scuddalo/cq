Paperclip.interpolates :to_param do |attachment, style|
  attachment.instance.to_param
end