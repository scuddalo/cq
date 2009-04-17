class CreateGroupMembers < ActiveRecord::Migration
  def self.up
    create_table :group_members do |t|
      t.integer :group_id
      t.integer :friendship_id

      t.timestamps
    end
  end

  def self.down
    drop_table :group_members
  end
end
