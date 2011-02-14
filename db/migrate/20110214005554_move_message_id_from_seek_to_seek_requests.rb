class MoveMessageIdFromSeekToSeekRequests < ActiveRecord::Migration
  def self.up
    add_column :seek_requests, :message_id, :integer, 
    remove_column :seeks, :message_id
  end

  def self.down
    remove_column :seek_requests, :message_id
    add_column :seeks, :message_id, :integer
  end
end
