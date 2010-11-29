class AddDefaultToStatusInInvitation < ActiveRecord::Migration
  def self.up
    change_column_default(:invitations, :status, false)
  end

  def self.down
  end
end
