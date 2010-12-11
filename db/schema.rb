# This file is auto-generated from the current state of the database. Instead of editing this file, 
# please use the migrations feature of Active Record to incrementally modify your database, and
# then regenerate this schema definition.
#
# Note that this schema.rb definition is the authoritative source for your database schema. If you need
# to create the application database on another system, you should be using db:schema:load, not running
# all the migrations from scratch. The latter is a flawed and unsustainable approach (the more migrations
# you'll amass, the slower it'll run and the greater likelihood for issues).
#
# It's strongly recommended to check this file into your version control system.

ActiveRecord::Schema.define(:version => 20100425004110) do

  create_table "favorite_locations", :force => true do |t|
    t.integer  "location_id"
    t.integer  "profile_id"
    t.string   "name"
    t.datetime "created_at"
    t.datetime "updated_at"
  end

  add_index "favorite_locations", ["location_id"], :name => "index_favorite_locations_on_location_id"
  add_index "favorite_locations", ["profile_id", "location_id"], :name => "index_favorite_locations_on_profile_id_and_location_id"
  add_index "favorite_locations", ["profile_id", "name"], :name => "index_favorite_locations_on_profile_id_and_name", :unique => true
  add_index "favorite_locations", ["profile_id"], :name => "index_favorite_locations_on_profile_id"

  create_table "friendships", :force => true do |t|
    t.integer  "follower_id"
    t.integer  "followee_id"
    t.integer  "tier",        :default => 3
    t.datetime "created_at"
    t.datetime "updated_at"
    t.boolean  "status"
  end

  add_index "friendships", ["followee_id", "follower_id"], :name => "index_friendships_on_followee_id_and_follower_id", :unique => true
  add_index "friendships", ["follower_id", "followee_id"], :name => "index_friendships_on_follower_id_and_followee_id", :unique => true

  create_table "group_members", :force => true do |t|
    t.integer  "group_id"
    t.integer  "friendship_id"
    t.datetime "created_at"
    t.datetime "updated_at"
  end

  create_table "groups", :force => true do |t|
    t.string   "name",       :limit => 40
    t.integer  "owner_id"
    t.datetime "created_at"
    t.datetime "updated_at"
  end

  create_table "invitations", :force => true do |t|
    t.integer  "from_profile_id"
    t.integer  "to_profile_id"
    t.boolean  "status",          :default => false
    t.datetime "created_at"
    t.datetime "updated_at"
  end

  create_table "locations", :force => true do |t|
    t.string   "address"
    t.string   "location_type"
    t.decimal  "latitude",      :precision => 11, :scale => 9
    t.decimal  "longitude",     :precision => 12, :scale => 9
    t.datetime "created_at"
    t.datetime "updated_at"
  end

  create_table "messages", :force => true do |t|
    t.text     "content"
    t.boolean  "read",       :default => false
    t.datetime "created_at"
    t.datetime "updated_at"
  end

  create_table "profiles", :force => true do |t|
    t.integer  "user_id"
    t.string   "display_name"
    t.text     "description"
    t.string   "email"
    t.string   "cell_number"
    t.string   "cell_carrier"
    t.integer  "location_id"
    t.datetime "created_at"
    t.datetime "updated_at"
    t.string   "status"
    t.binary   "picture"
    t.string   "photo_file_name"
    t.string   "photo_content_type"
    t.integer  "photo_file_size"
  end

  add_index "profiles", ["user_id"], :name => "index_profiles_on_user_id"

  create_table "seek_requests", :force => true do |t|
    t.integer "seek_id"
    t.integer "seeked_profile_id"
  end

  create_table "seek_responses", :force => true do |t|
    t.integer "seek_id"
    t.integer "responding_profile_id"
    t.integer "message_id"
    t.boolean "accept"
  end

  create_table "seeks", :force => true do |t|
    t.integer "owner_id"
    t.integer "message_id"
    t.boolean "is_active"
  end

  create_table "settings", :force => true do |t|
    t.integer  "profile_id"
    t.string   "name"
    t.string   "value"
    t.datetime "created_at"
    t.datetime "updated_at"
  end

  add_index "settings", ["profile_id", "name"], :name => "index_settings_on_profile_id_and_name", :unique => true

  create_table "users", :force => true do |t|
    t.string   "login",                     :limit => 40
    t.string   "crypted_password",          :limit => 40
    t.string   "salt",                      :limit => 40
    t.string   "remember_token"
    t.datetime "remember_token_expires_at"
    t.boolean  "is_admin",                                :default => false
    t.string   "email_verification"
    t.boolean  "email_verified",                          :default => false
    t.datetime "created_at"
    t.datetime "updated_at"
  end

  add_index "users", ["login"], :name => "index_users_on_login"

end
