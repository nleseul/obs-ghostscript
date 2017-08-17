# obs-ghostscript

## Introduction

The obs-ghostscript plugin is intended to allow an easy way to include a specific PDF document (or other type 
supported by the [Ghostscript](https://ghostscript.com/) library) in your [OBS Studio](https://obsproject.com/) 
scenes. It should be a simpler alternative to using a window capture source to show the contents of an Acrobat 
window.

The document is shown one page at a time, and the current page can be updated real-time through an interaction
window to scroll through a document live during a capture session. 

## Installation

The binary package mirrors the structure of the OBS Studio installation directory, so you should be able to
just drop its contents alongside an OBS Studio install (usually at C:\Program Files (x86)\obs-studio\). The 
necessary files should look like this: 

    obs-studio
    |---data
    |   |---obs-plugins
    |       |---obs-ghostscript
    |           |---locale
    |               |---en-US.ini
    |---obs-plugins
        |---32bit
        |   |---gsdll32.dll
        |   |---obs-ghostscript.dll
        |---64bit
            |---gsdll64.dll
            |---obs-ghostscript.dlll

## Usage

The source is called "PDF Document (Ghostscript)" in OBS Studio's source creation menu. The properties to set
on it are the path to the document file and the page from the document which should be shown. 

By right-clicking on the source and choosing "Interact", you can manipulate the current page more quickly while
recording/streaming than the source properties dialog allows. With the source displayed in an interaction window,
you can scroll your mouse wheel and change the displayed page. (Note that the source will only receive mouse 
events if the mouse is hovered over the image shown in the intraction window.)

## Building

If you wish to build the obs-ghostscript plugin from source, you should just need [CMake](https://cmake.org/), 
the OBS Studio libraries and headers, and the Ghostscript libraries and headers. 

* [obs-ghostscript source repository](https://github.com/nleseul/obs-ghostscript)
* [OBS studio source repository](https://github.com/jp9000/obs-studio)
* [Ghostscript source repository](http://git.ghostscript.com/?p=ghostpdl.git;a=summary)

I don't believe that the OBS project provides prebuilt libraries; you're probably going to have the best luck
building your own OBS binaries from the source. Refer to the OBS repository for more information on that.

The [standard Ghostscript installers](https://www.ghostscript.com/download/gsdnld.html) do provide binary 
libraries for linking, but they don't provide the Ghostscript header files. You may have the best luck
downloading the binary release for the library, and then harvesting the headers from the source release. 
You might also be okay just harvesting the necessary headers from Ghostscript's web documentation; 
obs-ghostscript only uses two headers&mdash;[psi/api.h](https://www.ghostscript.com/doc/psi/iapi.h) and 
[devices/gdevdsp.h](https://www.ghostscript.com/doc/devices/gdevdsp.h). If you download those individually,
ensure that they are arranged in the proper folders.

When building in CMake, you will probably need to set three configuration values so your environment can be
properly set up:

* OBSSourcePath should refer to the libobs subfolder in the OBS source release. The build pipeline will look
  for headers in this location, and for libraries in a "build" folder relative to that path (where the OBS 
  build process puts them). 
* GSSourcePath should refer to the root folder of a Ghostscript source distribution (or a subset thereof 
  containing the two aforementioned headers).
* GSLibraryPath should refer to a folder containing the gsdll[32|64].lib binary libraries. 

Note that the build pipeline will not copy the generated DLLs to an OBS Studio installation; you will need 
to do that manually, to the locations described above. Make sure to copy the gsdll[32|64].dll from the 
Ghostscript binary folder as well. 

## License

This project is licensed under the "[Unlicense](http://unlicense.org/)", because copy[right|left] is a hideous
mess to deal with and I don't like it. 

The Ghostscript binaries included in binary distributions of this project are copyright [Artifex Software, 
Inc.](https://www.ghostscript.com/Licensing.html) and licensed under the GNU Affero General 
Public License (AGPL). The source code is available from locations previously listed in this document. 
Refer to the Ghostscript project's [licensing information](https://www.ghostscript.com/Licensing.html)
for more details. 
