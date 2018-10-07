YUView uses the project libde265 to enable decoding of h.265/HEVC sequences. The library is loaded at runtime using QLibrary. If you wish to replace the libde265 version you can just swap the files in this directory or use the decoder selection setting in YUView.

You can find more information on the libde265 page:

http://www.libde265.org/

The library `libde265` is distributed under the terms of the GNU Lesser
General Public License. A copy of the licence can be found in the LICENCE.TXT file. The full source code that was used to build the precompiled versions of libde265 can be found in the libde265_source.zip file. If you are unable to obtain the libde265 source code you can also contact us at post@ient.rwth-aachen.de. The libraries were built using appveyor/travis. The full source code and compiled libraries are also available in out libde265 Github repository: https://github.com/ChristianFeldmann/libde265

File explanation:
 de265.h / de265-version.h: 
	The header files to include which have the public exports of the library.
 libde265_source.zip: 
	The sources that were used to compile the precompile versions of the libde265 decoder.
	
 -- Windows:
  libde265_internals.dll: 
    The windows libraries were compiled using VS2012 in release. The windows runtime is linked statically.
	The windows dll which will be loaded at runtime. This has been compiled with SSE/SSE2 operations enabled and 64 bit.
	If your processor does not support these the libde265 decoder will probably not run.
	
 -- MAC
  libde265_internals.dylib: 
	The mac dynamic library. This has been compiled in 64-bit and with SSE/SSE2 operations enabled.
	
 -- Linux
  libde265_internals.so:
	The shared library for linux x64.
  