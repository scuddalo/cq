class AddOfflineToProfiles < ActiveRecord::Migration
  def self.up
    add_column :profiles, :offline, :boolean
  end

  def self.down
    remove_column :profiles, :offline
  end
end
