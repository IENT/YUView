YUView uses the project libde265 to enable decoding of h.265/HEVC sequences. If you want to compile YUView without libde265 use the YUVIEW_DISABLE_LIBDE265 macro. If you wish to replace the libde265 version you can just swapt the files in this directory.

You can find more information on the libde265 page:

http://www.libde265.org/

The library `libde265` is distributed under the terms of the GNU Lesser
General Public License. A copy of the licence can be found in the LICENCE.TXT file. The full source code that was used to build the precompiled versions of libde265 can be found in the libde265_source.zip file. If you are unable to obtain the libde265 source code you can also contact us at post@ient.rwth-aachen.de. 

File explanation:
 de265.h / de265-version.h: 
	The header files to include which have the public exports of the library.
 libde265_source.zip: 
	The sources that were used to compile the precompile versions of the libde265 decoder.
	
 -- Windows:
  libde265.dll: 
    The windows libraries were compiled using VS2012 in release. The windows runtime is linked statically.
	The windows dll which will be loaded at runtime. This has been compiled with SSE/SSE2 operations enabled and 64 bit.
	If your processor does not support these the libde265 decoder will probably not run.
	
 -- MAC
  libde265.0.dylib: 
	The mac dynamic library. This has been compiled in 64-bit and with SSE/SSE2 operations enabled.
	
 -- Linux
  libde265.so:
	The shared library for linux x64.
  libde265.so.0:
	A dynamic link to libde265.so