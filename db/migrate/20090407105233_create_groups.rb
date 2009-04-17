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


class AddVatAndDisplayColumns < ActiveRecord::Migration
def self.up   
   add_column :companies, :help, :int, :default => 1
   add_column :jobs, :start_job, :date, :default => '2007-1-1'
   add_column :jobs, :display, :boolean, :default => true
end
 def self.down    
    remove_column :companies, :vat_quarter
    remove_column :jobs, :start_job 
    remove_column :jobs, :display
  end
end
