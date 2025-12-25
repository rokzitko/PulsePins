# Optionally include local overrides (not tracked by git)
if {[file exists "local.qsf"]}  {
    puts "Applying local overrides from local.qsf"
    source local.qsf
  }
