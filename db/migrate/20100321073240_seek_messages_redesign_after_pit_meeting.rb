class SeekMessagesRedesignAfterPitMeeting < ActiveRecord::Migration
  def self.up
    remove_column :messages, :from_profile_id
    remove_column :messages, :to_profile_id
    remove_column :messages, :is_accepted
    create_table :seeks do |t|
      t.integer :owner_id, :message_id
      t.boolean :is_active
    end
    create_table :seek_requests do |t|
      t.integer :seek_id, :seeked_profile_id
    end
    create_table :seek_responses do |t|
      t.integer :seek_id, :responding_profile_id, :message_id
      t.boolean :accept
    end
    add_column :profiles, :image_path, :string
  end

  def self.down
    add_column :messages, :from_profile_id, :integer 
    add_column :messages, :to_profile_id, :integer 
    add_column :messages, :is_accepted, :boolean
    drop_table :seeks, :seek_requests, :seek_responses
    remove_column :profiles, :image_path
  end
end
