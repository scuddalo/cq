class AddCreatedModifiedToSeekRelatedTables < ActiveRecord::Migration
  def self.up
    add_column  :seeks, :created_at, :datetime
    add_column :seeks, :updated_at, :datetime
    add_column  :seek_requests, :created_at, :datetime
    add_column :seek_requests, :updated_at, :datetime
    add_column  :seek_responses, :created_at, :datetime
    add_column :seek_responses, :updated_at, :datetime
    add_column  :push_activities, :created_at, :datetime
    add_column :push_activities, :updated_at, :datetime
  end

  def self.down
  end
end
