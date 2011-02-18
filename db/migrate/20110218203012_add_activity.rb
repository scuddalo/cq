class AddActivity < ActiveRecord::Migration
  def self.up
    create_table :push_activities do |t|
      t.string :activity_type, :default => 'push'
      t.datetime :activity_date
    end
    
    PushActivity.create :activity_type => 'push', :activity_date => Time.now
  end

  def self.down
    drop_table :push_activities
  end
end
