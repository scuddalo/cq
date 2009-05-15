class CreateInvitations < ActiveRecord::Migration
  def self.up
    create_table :invitations do |t|
      t.integer :from_profile_id
      t.integer :to_profile_id
      t.boolean :status

      t.timestamps
    end
  end

  def self.down
    drop_table :invitations
  end
end
