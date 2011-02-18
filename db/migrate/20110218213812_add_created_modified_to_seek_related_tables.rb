class AddCreatedModifiedToSeekRelatedTables < ActiveRecord::Migration
  def self.up
    change_table :seeks do |t|
      t.datetime :created_at
      t.datetime :updated_at
    end
    change_table :seek_requests do |t|
      t.datetime :created_at
      t.datetime :updated_at
    end
    change_table :seek_responses do |t|
      t.datetime :created_at
      t.datetime :updated_at
    end
    change_table :push_activities do |t|
      t.datetime :created_at
      t.datetime :updated_at
    end
    
  end

  def self.down
  end
end
