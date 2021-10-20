#ifndef _GCC_PY_SETTINGS_H_
#define _GCC_PY_SETTINGS_H_

#ifdef MINGW
	/**
	 * \def COMPILE_ON_MINGW This flag will enable specific code for windows/mingw (cross)compilation
	 */
    // as we only support mingw on windows, COMPILE_ON_MINGW = COMPILE_ON_WINDOWS
	#define COMPILE_ON_MINGW
    #define PLUGIN_API __declspec(dllexport) __attribute__((visibility("default")))
	#define PLUGIN_API_TYPEDEF __declspec(dllexport)
#else
	// on unix/linux variants
	#define PLUGIN_API __attribute__((visibility("default")))
	#define PLUGIN_API_TYPEDEF __attribute__((visibility("default")))
#endif // end if MINGW 

#endif