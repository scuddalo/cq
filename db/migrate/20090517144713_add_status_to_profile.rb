class AddStatusToProfile < ActiveRecord::Migration
  def self.up
    add_column :profiles, :status, :string
  end

  def self.down
    add_column :profiles, :status
  end
end
