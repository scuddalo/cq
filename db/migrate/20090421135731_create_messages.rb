class CreateMessages < ActiveRecord::Migration
  def self.up
    create_table :messages do |t|
      t.integer :from_profile_id
      t.integer :to_profile_id
      t.text :content
      t.boolean :read, :default => false
      t.boolean :is_accepted

      t.timestamps
    end
  end

  def self.down
    drop_table :messages
  end
end
