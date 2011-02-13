#

def sign (cabfile)	
	puts "Singing .cab file"
	
	cabsigntool = $cabwiz[0, $cabwiz.index("CabWiz")] + "Security\\CabSignTool\\cabsigntool" if $config["env"]["paths"]["cabwiz"]
	cabsigntool = "cabsigntool" if cabsigntool.nil?
	
	signature = $config["build"]["wmsign"]
	cab_in  = cabfile
	cab_out = cabfile
	
	#TODO: need better solution, maybe just let know users on wiki to add bin dir of MS SDKs to PATH variable
	#Assuming that MS SDKs intalled to default path on c: or d: disk
	if Dir.exists?("C:\\Program Files\\Microsoft SDKs\\Windows\\v6.0A\\bin")
		ENV['PATH'] = ENV['PATH'] + "C:\\Program Files\\Microsoft SDKs\\Windows\\v6.0A\\bin" + ";"
	end
	if Dir.exists?("D:\\Program Files\\Microsoft SDKs\\Windows\\v6.0A\\bin")
		ENV['PATH'] = ENV['PATH'] + "D:\\Program Files\\Microsoft SDKs\\Windows\\v6.0A\\bin" + ";"
	end
	
	if system(cabsigntool + " "  + cab_in + " " + cab_out + " -F " + signature)
		puts "\nSigned successfully.\n\n"
	else
		puts "\nFailed to sign .cab file!\n\n"
	end
	
	$stdout.flush
end

namespace "config" do
  task :set_wm_platform do
    $current_platform = "wm" unless $current_platform
  end
  
  task :set_win32_platform do
    $current_platform = "win32" unless $current_platform
  end
  

  task :wm => [:set_wm_platform, "config:common"] do
    $rubypath = "res/build-tools/RhoRuby.exe" #path to RubyMac
    $builddir = $config["build"]["wmpath"] + "/build"
    $vcbindir = $config["build"]["wmpath"] + "/bin"
    $appname = $app_config["name"].nil? ? "Rhodes" : $app_config["name"] 
    $bindir = $app_path + "/bin"
    $rhobundledir =  $app_path + "/RhoBundle"
    $srcdir =  $bindir + "/RhoBundle"
    $targetdir = $bindir + "/target/wm6p"
    $tmpdir =  $bindir +"/tmp"
    $vcbuild = $config["env"]["paths"]["vcbuild"]
    $vcbuild = "vcbuild" if $vcbuild.nil?
    $cabwiz = File.join($config["env"]["paths"]["cabwiz"], "cabwiz.exe") if $config["env"]["paths"]["cabwiz"]
    $cabwiz = "cabwiz" if $cabwiz.nil?
    $sdk = "Windows Mobile 6 Professional SDK (ARMV4I)"
    $sdk = $app_config["wmsdk"] unless $app_config["wmsdk"].nil?

    $excludelib = ['**/builtinME.rb','**/ServeME.rb','**/dateME.rb','**/rationalME.rb']

    #$app_config["extensions"] = [] unless $app_config["extensions"].is_a? Array
    #if $app_config["wm"] and $app_config["wm"]["extensions"]
    #  $app_config["extensions"] += $app_config["wm"]["extensions"]
    #  $app_config["wm"]["extensions"] = nil
    #end
  end
end

namespace "build" do
  namespace "wm" do
    task :extensions => "config:wm" do
      $app_config["extensions"].each do |ext|
        $app_config["extpaths"].each do |p|
          extpath = File.join(p, ext, 'ext')
          next unless File.exists? File.join(extpath, "build.bat")

          ENV['RHO_PLATFORM'] = 'wm'
          ENV['PWD'] = $startdir
          ENV['RHO_ROOT'] = ENV['PWD']
          ENV['TARGET_TEMP_DIR'] = File.join(ENV['PWD'], "platform", "wm", "bin", $sdk, "rhodes", "Release")
          ENV['TEMP_FILES_DIR'] = File.join(ENV['PWD'], "platform", "wm", "bin", $sdk, "extensions", ext)
          ENV['VCBUILD'] = $vcbuild
          ENV['SDK'] = $sdk

          puts Jake.run("build.bat", [], extpath)
          break
        end
      end
    end

#    desc "Build wm rhobundle"
    task :rhobundle => ["config:wm", :extensions] do
      Rake::Task["build:bundle:noxruby"].execute
    end

    task :rhodes => ["config:wm", "build:wm:rhobundle"] do
      chdir $config["build"]["wmpath"]

      args = ['/M4', 'rhodes.sln', "\"Release|#{$sdk}\""]
      puts "\nThe following step may take several minutes or more to complete depending on your processor speed\n\n"
      puts Jake.run($vcbuild,args)
      unless $? == 0
        puts "Error building"
        exit 1
      end
      chdir $startdir
    end

    task :devrhobundle => ["config:set_wm_platform", "win32:devrhobundle"]
    end
  
  namespace "win32" do

    task :extensions => "config:wm" do
      $app_config["extensions"].each do |ext|
        $app_config["extpaths"].each do |p|
          extpath = File.join(p, ext, 'ext')
          next unless File.exists? File.join(extpath, "build.bat")

          ENV['RHO_PLATFORM'] = 'win32'
          ENV['PWD'] = $startdir
          ENV['RHO_ROOT'] = ENV['PWD']
          ENV['TARGET_TEMP_DIR'] = File.join(ENV['PWD'], "platform", "wm", "bin", "win32", "rhodes", "Debug")
          ENV['TEMP_FILES_DIR'] = File.join(ENV['PWD'], "platform", "wm", "bin", "win32", "extensions", ext)
          ENV['VCBUILD'] = $vcbuild
          ENV['SDK'] = $sdk

          puts Jake.run("build.bat", [], extpath)
          break
        end
      end
    end

#    desc "Build win32 rhobundle"
    task :rhobundle => ["config:wm", :extensions] do
      Rake::Task["build:bundle:noxruby"].execute
    end


  
    task :devrhobundle => ["config:set_win32_platform", :rhobundle] do
        win32rhopath = 'platform/wm/bin/win32/rhodes/Debug/rho/'
        mkdir_p win32rhopath
        namepath = File.join(win32rhopath,"name.txt")        
        old_appname = File.read(namepath) if File.exists?(namepath)

        confpath = File.join(win32rhopath,"apps/rhoconfig.txt.changes")        
        confpath_content = File.read(confpath) if File.exists?(confpath)
        
        rm_rf win32rhopath + 'lib'      
        rm_rf win32rhopath + 'apps'
        rm_rf win32rhopath + 'db' if old_appname != $appname
        
        cp_r $srcdir + '/lib', win32rhopath
        cp_r $srcdir + '/apps', win32rhopath      
        cp_r $srcdir + '/db', win32rhopath      
        
        File.open(namepath, "w") { |f| f.write($appname) }      
        File.open(confpath, "w") { |f| f.write(confpath_content) }  if old_appname == $appname && confpath_content && confpath_content.length()>0
        
    end
  end

  #desc "Build rhodes for win32"
  task :win32 => ["config:set_win32_platform", "config:wm", "build:win32:devrhobundle"] do
    chdir $config["build"]["wmpath"]

    args = ['/M4', 'rhodes.sln', '"debug|win32"']
    puts "\nThe following step may take several minutes or more to complete depending on your processor speed\n\n"
    puts Jake.run($vcbuild,args)
    
    chdir $startdir
    
    unless $? == 0
      puts "Error building"
      exit 1
    end
  end
end

namespace "device" do
  namespace "wm" do
    desc "Build production for device or emulator"
    task :production => ["config:wm","build:wm:rhobundle","build:wm:rhodes"] do
	
	  out_dir = $startdir + "/" + $vcbindir + "/#{$sdk}" + "/rhodes/Release/"
      cp  out_dir + "rhodes.exe", out_dir + $appname + ".exe" 
	
      chdir $builddir
      
      cp $app_path + "/icon/icon.ico", "../rhodes/resources" if File.exists? $app_path + "/icon/icon.ico"

      args = ['build_inf.js', $appname + ".inf", 'wm6', '"' + $app_config["name"] +'"', $app_config["vendor"], '"' + $srcdir + '"', $hidden_app]
      puts Jake.run('cscript',args)
      unless $? == 0
        puts "Error running build_inf"
        exit 1
      end        
      
      args = [$appname + ".inf"]
      puts Jake.run($cabwiz, args)
      unless $? == 0
        puts "Error running cabwiz"
        exit 1
      end        
      
      args = ['cleanup.js']
      puts Jake.run('cscript',args)
      unless $? == 0
        puts "Error running cleanup.js"
        exit 1
      end    

      mkdir_p $bindir if not File.exists? $bindir
      mkdir_p $targetdir if not File.exists? $targetdir
      mv $appname + ".inf", $targetdir
      mv $appname + ".cab", $targetdir

      File.open(File.join($targetdir,"app_info.txt"), "w") { |f| f.write( $app_config["vendor"] + " " + $appname + "/" + $appname + ".exe") }
            
	  if (not $config["build"]["wmsign"].nil?) and $config["build"]["wmsign"] != ""
        sign $targetdir + '/' +  $appname + ".cab";
	  end
	  
      rm_f "cleanup.js"

      chdir $startdir

    end
  end
end

namespace "clean" do

  desc "Clean wm"
  task :wm => "clean:wm:all"
  namespace "wm" do
    task :rhodes => ["config:wm"] do
      rm_rf $vcbindir + "/#{$sdk}"
      rm_rf $targetdir
    end
    task :all => "clean:wm:rhodes"
  end

  desc "Clean win32"
  task :win32 => [ "config:wm" ]do
    rm_rf $vcbindir + "/win32"
    #rm_rf $targetdir
  end
end

namespace "run" do

  namespace "wm" do

    desc "Build and run on the Windows Phone"
    task :dev => ["device:wm:production"] do
   	  cd $startdir + "/res/build-tools"
	  detool = "detool.exe"    
	  args   = [ 'dev', $appname, $srcdir, $startdir + "/" + $vcbindir + "/#{$sdk}" + "/rhodes/Release/" + $appname + ".exe" ]
	  puts "\nStarting application on the device"
	  puts "Please, connect you device via ActiveSync.\n\n"
	  Jake.run(detool,args)
    end

    desc "Build and run on WM6 emulator"
    task :emu => ["device:wm:production"] do
   	  cd $startdir + "/res/build-tools"
	  detool = "detool.exe"    
	  args   = [ 'emu', '"Windows Mobile 6 Professional Emulator"', $appname, $srcdir, $startdir + "/" + $vcbindir + "/#{$sdk}" + "/rhodes/Release/" + $appname + ".exe" ]
	  puts "\nStarting application on the WM6 emulator\n\n"
	  Jake.run(detool,args)
    end

    desc "Build, install .cab  and run on the Windows Phone"
    task :devcab => ["device:wm:production"] do
   	  cd $startdir + "/res/build-tools"
	  detool = "detool.exe"    
	  args   = ['devcab', $targetdir + '/' +  $appname + ".cab", $appname]
	  puts "\nStarting application on the device"
	  puts "Please, connect you device via ActiveSync.\n\n"
	  Jake.run(detool,args)
    end

    desc "Build, install .cab and run on WM6 emulator"
    task :emucab => ["device:wm:production"] do
  	  cd $startdir + "/res/build-tools"
	  detool = "detool.exe"
	  args   = ['emucab', '"Windows Mobile 6 Professional Emulator"', $targetdir + '/' +  $appname + ".cab", $appname]
	  puts "\nStarting application on the WM6 emulator\n\n"
	  Jake.run(detool,args)
    end
  end
	
  desc "Run win32" 
  task :win32 => ["build:win32"] do
    args = [' ']
#    chdir $config["build"]["wmpath"]
#    Thread.new { Jake.run("bin\\win32\\rhodes\\Debug\\rhodes", args) }
    Jake.run2 "bin\\win32\\rhodes\\Debug\\rhodes.exe", args, {:directory => $config["build"]["wmpath"], :nowait => true}
    
    $stdout.flush
    chdir $startdir
    
    unless $? == 0
      puts "Error to run rhodes for win32"
      exit 1
    end
  end
 
  namespace "win32" do

    task :delete_db do
        db_path = 'platform/wm/bin/win32/rhodes/Debug/rho/db'
        rm_rf db_path if File.exists?(db_path)    
    end
    
    task :spec => [:delete_db, "build:win32"] do
        #remove log file
        win32rhopath = 'platform/wm/bin/win32/rhodes/Debug/rho/'
        win32logpath = File.join(win32rhopath,"RhoLog.txt")        
        win32logpospath = File.join(win32rhopath,"RhoLog.txt_pos")        
        win32configpath = File.join(win32rhopath,"apps/rhoconfig.txt.changes")                
        rm_rf win32logpath if File.exists?(win32logpath)
        rm_rf win32logpospath if File.exists?(win32logpospath)
        rm_rf win32configpath if File.exists?(win32configpath)
        
        Jake.before_run_spec
        start = Time.now
        
        args = [' ']
        Jake.run2( "bin\\win32\\rhodes\\Debug\\rhodes.exe", args, {:directory => $config["build"]["wmpath"], :nowait => false}) do |line|
            Jake.process_spec_output(line)        
        end
        Jake.process_spec_results(start)        
        
        $stdout.flush
        chdir $startdir
    end

    task :phone_spec do
      exit 1 if Jake.run_spec_app('win32','phone_spec')
      exit 0
    end

    task :framework_spec do
      exit 1 if Jake.run_spec_app('win32','framework_spec')
      exit 0
    end
    
  end
  
end
