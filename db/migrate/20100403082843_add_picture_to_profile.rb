class AddPictureToProfile < ActiveRecord::Migration
  def self.up
    add_column :profiles, :picture, :bytea
    remove_column :profiles, :image_path
  end

  def self.down
    remove_column :profiles, :picture
    add_column :profiles, :image_path, :string
  end
end
