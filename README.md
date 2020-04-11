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
            |---obs-ghostscript.dll

Note that binary packages are currently only available for Windows; I'm really not sure how reliable a Linux
binary package would be on the wide variety of systems out there.

## Usage

The source is called "PDF Document (Ghostscript)" in OBS Studio's source creation menu. The properties to set
on it are the path to the document file and the page from the document which should be shown.

You can also enter the document password for password-protected PDFs. **OBS Studio will store this setting in 
plain text** in its local data files; if you're deeply concerned about someone breaking into your computer and
stealing your datas, you might not want to use this option.

Hotkeys are provided to modify the displayed page more quickly while recording/streaming than the source
properties dialog allows. These can be set through the main Hotkeys tab in the OBS Studio settings.

You can also manipulate the current page by right-clicking on the source and choosing "Interact".
With the source displayed in an interaction window, you can scroll your mouse wheel and change the
displayed page, or use the arrow keys or PgUp/PgDown keys. (Note that the source will only receive mouse
events if the mouse is hovered over the image shown in the intraction window.)

If the native size of the document is not appropriate for your use case, you can specify settings which
will control the size at which it will be rendered. Selecting "Override DPI" and changing the "DPI" option
will change the DPI (dots per inch) used when rendering. Increasing this will increase the size in pixels
of the document proportionally.

You can also specify the page size Ghostscript uses when interpreting the document by selecting "Override Page
Size" and specifying a width and height. These values are in points, not pixels; 1 point is 1/72 inch. The
conversion from points to rendered pixels is dependent on the DPI setting, either that contained in the document
or the one specified in the source properties. You can choose whether Ghostscript will scale the contents of the
document to fit this page size or not as well, but that option is specific to PDF documents.

## Building

If you wish to build the obs-ghostscript plugin from source, you should just need [CMake](https://cmake.org/),
the OBS Studio libraries and headers, and the Ghostscript libraries and headers.

* [obs-ghostscript source repository](https://github.com/nleseul/obs-ghostscript)
* [OBS studio source repository](https://github.com/jp9000/obs-studio)
* [Ghostscript source repository](http://git.ghostscript.com/?p=ghostpdl.git;a=summary)

I don't believe that the OBS project provides prebuilt libraries; you're probably going to have the best luck
building your own OBS binaries from the source. Refer to the OBS repository for more information on that.

The [standard Ghostscript installers](https://www.ghostscript.com/download/gsdnld.html) do provide binary
libraries for linking, but they don't provide the Ghostscript header files. On Windows, you may have the
best luck downloading the binary release for the library, and then harvesting the headers from the source
release. Linux environments should be able to install a libgs-dev package or something similar to acquire
both libraries and headers.

You might also be okay just harvesting the necessary headers from Ghostscript's web documentation;
obs-ghostscript only uses two headers&mdash;[psi/api.h](https://www.ghostscript.com/doc/psi/iapi.h) and
[devices/gdevdsp.h](https://www.ghostscript.com/doc/devices/gdevdsp.h). If you download those individually,
ensure that they are arranged in the proper folders. The Linux dev package would probably place them
directly in a `ghostscript` subfolder of a system include directory (e.g., /usr/include/ghostscript); the
CMake script should detect that arrangement of headers as well.

### Windows

When building in CMake, you will probably need to set four configuration values so your environment can be
properly set up:

* `OBSSourcePath` should refer to the libobs subfolder in the OBS source release.
* `OBSLibraryPath` should refer to the folder where the libobs library binaries are.
* `GSSourcePath` should refer to the root folder of a Ghostscript source distribution (or a subset thereof
  containing the two aforementioned headers).
* `GSLibraryPath` should refer to a folder containing the gsdll[32|64].lib binary libraries.

Installation logic is provided through CMake as well; you can set the `CMAKE_INSTALL_PREFIX` configuration value
to choose the folder to which the files will be copied. You can also manually copy all files to the locations
described above.

### Linux

Note that I've only tested this plugin briefly in Linux via an Ubuntu VM; I don't know how well it will work
in other distros, or what the performance is like on a real machine.

As with Windows, you will need to specify the `OBSSourcePath` and `OBSLibraryPath` variables on the CMake
command line. A Linux environment will probably have Ghostscript available in a standard system location,
if you've installed it through a package like libgs-dev in your package manager, so it shouldn't be necessary
to manually specify the paths to Ghostscript.

In my particular case, the build process looked like this, starting from the obs-ghostscript directory:

    mkdir build64
    cd build64
    cmake .. -DOBSSourcePath=~/Development/External/obs-studio/libobs/ -DOBSLibraryPath=~/obs-studio/bin/64bit/ -DCMAKE_INSTALL_PREFIX=~/obs-studio
    make
    make install

Your Linux may vary.

## License

This project is licensed under the "[Unlicense](http://unlicense.org/)", because copy[right|left] is a hideous
mess to deal with and I don't like it.

The Ghostscript binaries included in binary distributions of this project are copyright [Artifex Software,
Inc.](https://www.ghostscript.com/Licensing.html) and licensed under the GNU Affero General
Public License (AGPL). The source code is available from locations previously listed in this document.
Refer to the Ghostscript project's [licensing information](https://www.ghostscript.com/Licensing.html)
for more details.
