YUView uses the project libde265 to enable decoding of h.265/HEVC sequences. If you want to compile YUView without libde265 use the YUVIEW_DISABLE_LIBDE265 macro. If you wish to replace the libde265 version you can just swapt the files in this directory.

You can find more information on the libde265 page:

http://www.libde265.org/

The library `libde265` is distributed under the terms of the GNU Lesser
General Public License. A copy of the licence can be found in the LICENCE.TXT file. The full source code that was used to build the precompiled versions of libde265 can be found in the libde265_source.zip file. If you are unable to obtain the libde265 source code you can also contact us at post@ient.rwth-aachen.de. 

File explanation:
 de265.h / de265-version.h: 
	The header files to include which have the public exports of the library.
 libde265.lib: 
	The windows library to link against. 
 libde265.dll: 
	The windows dll which will be loaded at runtime. This has been compiled with SSE/SSE2 operations enabled and 64 bit.
	If your processor does not support these the libde265 decoder will probably not run.
 libde265_source.zip: The sources that were used to compile the precompile versions of the libde265 decoder.