import gcc

gcc.register_callback(gcc.EVENT.PLUGIN_PASS_EXECUTION, lambda gcc_pass: print('exec pass:', gcc_pass) )