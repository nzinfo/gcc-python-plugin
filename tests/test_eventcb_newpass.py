import gcc

gcc.register_callback(gcc.EVENT.PLUGIN_NEW_PASS, lambda gcc_pass: print('include file:', dir(gcc_pass)) )