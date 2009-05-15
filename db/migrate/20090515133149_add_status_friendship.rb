class AddStatusFriendship < ActiveRecord::Migration
  def self.up
    add_column :friendships, :status, :boolean
  end

  def self.down
    remove_column :friendships, :status
  end
end
