class CreateGroups < ActiveRecord::Migration
  def self.up
    create_table :groups, :force => true do |t|
  		t.string  :name,  :limit => 40, :nil => false
			t.integer :owner_id, :nil => false
	
			t.timestamps	
    end
  end

  def self.down
    drop_table :groups
  end
end
