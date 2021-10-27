import gcc

gcc.register_callback(gcc.EVENT.PLUGIN_INCLUDE_FILE, lambda s: print('include file:', s) )