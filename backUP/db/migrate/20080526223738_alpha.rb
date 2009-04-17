class Alpha < ActiveRecord::Migration
  def self.up
    create_table 'users', :force => true do |t|
      t.string    'login',                    :limit => 40, :nil => false
      t.string    'crypted_password',         :limit => 40, :nil => false
      t.string    'salt',                     :limit => 40, :nil => false
      t.string    'remember_token'
      t.datetime  'remember_token_expires_at'
      t.boolean   'is_admin',                 :default => false
      t.string    'email_verification'
      t.boolean   'email_verified',           :default => false
      t.timestamps
    end

    add_index 'users', ['login'], :name => 'index_users_on_login'
    
    create_table 'profiles', :force => true do |t|
      t.belongs_to  :user
      t.string      'display_name'
      t.text        'description'
      t.string      'email',                  :nil => false
      t.string      'cell_number'
      t.string      'cell_carrier'
      t.belongs_to  :location
      t.timestamps
    end

    add_index 'profiles', 'user_id', :name => 'index_profiles_on_user_id'

    create_table :locations do |t|
      t.string    'address',                  :nil => false
      t.string    'location_type',            :nil => false
      t.decimal   'latitude',                 :precision => 11, :scale => 9  #  xy.abcdefghi
      t.decimal   'longitude',                :precision => 12, :scale => 9  # xyz.abcdefghi
      t.timestamps
    end
    
    create_table :favorite_locations do |t|
      t.belongs_to  :location,                :nil => false
      t.belongs_to  :profile,                 :nil => false
      t.string      'name',                   :nil => false
      t.timestamps
    end
    
    add_index 'favorite_locations', 'profile_id', :name => 'index_favorite_locations_on_profile_id', :unique => false
    add_index 'favorite_locations', 'location_id', :name => 'index_favorite_locations_on_location_id', :unique => false
    add_index 'favorite_locations', ['profile_id', 'location_id'], :name => 'index_favorite_locations_on_profile_id_and_location_id', :unique => false
    add_index 'favorite_locations', ['profile_id', 'name'], :name => 'index_favorite_locations_on_profile_id_and_name', :unique => true
    
    create_table "friendships", :force => true do |t|
      t.belongs_to  :follower,                :nil => false
      t.belongs_to  :followee,                :nil => false
      t.integer     'tier',                   :nil => false, :default => 3
      t.timestamps
    end

    add_index "friendships", ["follower_id", "followee_id"], :name => "index_friendships_on_follower_id_and_followee_id", :unique => true
    add_index "friendships", ["followee_id", "follower_id"], :name => "index_friendships_on_followee_id_and_follower_id", :unique => true
    
    create_table 'settings', :force => true do |t|
      t.belongs_to  :profile,                 :nil => false
      t.string      'name',                   :nil => false
      t.string      'value',                  :nil => false
      t.timestamps
    end
    
    add_index 'settings', ['profile_id', 'name'], :name => 'index_settings_on_profile_id_and_name', :unique => true
  end

  def self.down
    drop_table :settings
    drop_table :friendships
    drop_table :locations
    drop_table :profiles
    drop_table :users
  end
end
