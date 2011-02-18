class AddActivity < ActiveRecord::Migration
  def self.up
    create_table :push_activities do |t|
      t.string :activity_type, :default => 'push'
      t.datetime :activity_date
    end
  end

  def self.down
    drop_table :push_activities
  end
end
