import gcc

gcc.register_callback(gcc.PLUGIN_PASS_EXECUTION, lambda gcc_pass: print('exec pass:', gcc_pass) )