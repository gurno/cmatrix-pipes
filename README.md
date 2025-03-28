<h1 align="center">CMatrix-Pipes</h1>

<h3 align="center"> Matrix like effect in your terminal with named pipe control </h3>

<p align="center">
  <strong>Fork of <a href="https://github.com/abishekvashok/cmatrix">CMatrix</a> with named pipe control features</strong>
</p>

<div align="center">
  <table>
    <tr>
      <td align="center">
        <a href="https://github.com/gurno/cmatrix-pipes/actions/workflows/c-cpp.yml">
          <img src="https://github.com/gurno/cmatrix-pipes/actions/workflows/c-cpp.yml/badge.svg" alt="C/C++ CI">
        </a><br>
        <a href="https://github.com/gurno/cmatrix-pipes/actions/workflows/makefile.yml">
          <img src="https://github.com/gurno/cmatrix-pipes/actions/workflows/makefile.yml/badge.svg" alt="Makefile CI">
        </a><br>
        <a href="https://github.com/gurno/cmatrix-pipes/actions/workflows/codeql.yml">
          <img src="https://github.com/gurno/cmatrix-pipes/actions/workflows/codeql.yml/badge.svg" alt="CodeQL Advanced">
        </a>
      </td>
      <td align="center">
        <a href="./COPYING">
          <img src="https://img.shields.io/github/license/gurno/cmatrix-pipes?color=blue" alt="License">
        </a><br>
        <img src="https://img.shields.io/badge/contributions-welcome-orange" alt="Contributions Welcome">
      </td>
    </tr>
  </table>
</div>


![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## Contents
- [Overview](#cloud-overview)
- [Build Dependencies](#open_file_folder-build-dependencies)
- [Building and Installation](#floppy_disk-building-and-installing-cmatrix-pipes)
    - [Using configure](#small_blue_diamond-using-configure-recommended-for-most-linuxmingw-users)
    - [Using CMake](#small_blue_diamond-using-cmake)
- [Usage](#bookmark_tabs-usage)
    - [Named Pipe Control](#named-pipe-control)
- [Captures](#camera-captures)
    - [Screenshots](#small_blue_diamond-screenshots)
    - [Screencasts](#small_blue_diamond-screencasts)
- [Original Project](#link-original-project)
- [Contribution Guide](#book-contribution-guide)
- [License](#page_facing_up-license)

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :cloud: Overview

CMatrix-Pipes is a fork of the original [CMatrix](https://github.com/abishekvashok/cmatrix) with added support for runtime control through named pipes. Like the original, it shows text flying in and out in a terminal as seen in "The Matrix" movie, but with the additional ability to change colors and speed while running through a named pipe interface.

The original CMatrix is based on the screensaver from The Matrix website. It can scroll lines all at the same rate or asynchronously and at a user-defined speed.

CMatrix-Pipes maintains all the features of the original while adding new runtime control capabilities.

> :grey_exclamation:`Disclaimer` : We are in no way affiliated in any way with the movie "The Matrix", "Warner Bros" nor
any of its affiliates in any way, just fans.

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :open_file_folder: Build Dependencies
You'll need the following libraries to build cmatrix-pipes:

1. **ncurses library** - For terminal manipulation. On Windows, using mingw-w64-ncurses is recommended (PDCurses will also work, but it does not support colors or bold text).

2. **pthread library** - For named pipe support.

##### :small_blue_diamond: For Linux<br>
Run these commands to check if you have the required libraries:
```
ldconfig -p | grep ncurses
ldconfig -p | grep pthread
```
If you get no output for ncurses, you need to install it. Click below for installation instructions:
- [ncurses](https://www.cyberciti.biz/faq/linux-install-ncurses-library-headers-on-debian-ubuntu-centos-fedora/)

Most Linux distributions include pthread by default. If missing, you can install it with your package manager:
```
# For Debian/Ubuntu
sudo apt-get install libpthread-stubs0-dev

# For Fedora/CentOS
sudo dnf install glibc-devel
```

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :floppy_disk: Building and installing CMatrix-Pipes
To install cmatrix-pipes, clone this repo in your local system and use either of the following methods from within the cmatrix-pipes directory.

#### :small_blue_diamond: Using configure (recommended for most linux/mingw users)
```sh
autoreconf -i  # skip if using released tarball
./configure
make
make install
```

#### :small_blue_diamond: Using CMake
Here we also show an out-of-source build in the sub directory "build".
(Doesn't work on Windows, for now).
```sh
mkdir -p build
cd build
# to install to "/usr/local"
cmake ..
# OR 
# to install to "/usr"
#cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
make install
```

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :bookmark_tabs: Usage

After you have installed **cmatrix-pipes** just type the command `cmatrix-pipes` to run it :)
```sh
cmatrix-pipes
```
Run with different arguments to get different effects.
```sh
cmatrix-pipes [-abBflohnsmVx] [-u update] [-C color] [-P pipe_path]
```
Example:
```sh
cmatrix-pipes -ba -u 2 -C red
```

For more options and **help** run `cmatrix-pipes -h` <br>OR<br> Read Manual Page by running command `man cmatrix`

_To get the program to look most like the movie, use `cmatrix-pipes -lba`_
_To get the program to look most like the Win/Mac screensaver, use `cmatrix-pipes -ol`_

### Named Pipe Control

CMatrix-Pipes supports runtime control through a named pipe with the `-P` option:

```sh
cmatrix-pipes -P /tmp/cmatrix_pipe
```

This creates a named pipe at the specified path. You can control the animation while it's running by sending commands to this pipe from another terminal or script:

```sh
# Change color to red
echo "color=red" > /tmp/cmatrix_pipe

# Change to rainbow mode
echo "color=rainbow" > /tmp/cmatrix_pipe

# Change speed (0-10, lower is faster)
echo "speed=2" > /tmp/cmatrix_pipe

# Pause the animation
echo "pause=on" > /tmp/cmatrix_pipe

# Resume the animation
echo "pause=off" > /tmp/cmatrix_pipe

# Change bold style
echo "bold=all" > /tmp/cmatrix_pipe

# Exit cmatrix-pipes (doesn't work in lock mode)
echo "exit=force" > /tmp/cmatrix_pipe
```

#### Available Pipe Commands:

Command | Description | Examples
--------|-------------|--------
`color=VALUE` | Change matrix color or enable rainbow mode | `color=green`<br>`color=red`<br>`color=blue`<br>`color=white`<br>`color=yellow`<br>`color=cyan`<br>`color=magenta`<br>`color=black`<br>`color=rainbow`
`speed=VALUE` | Set animation speed (0-10, where 0 is fastest) | `speed=0`<br>`speed=5`<br>`speed=10`
`pause=VALUE` | Pause or resume animation | `pause=on`<br>`pause=off`<br>`pause=1`<br>`pause=0`<br>`pause=true`<br>`pause=false`
`bold=VALUE` | Control character boldness | `bold=on`<br>`bold=off`<br>`bold=all`<br>`bold=0`<br>`bold=1`<br>`bold=2`
`exit=VALUE` | Exit CMatrix (won't work in lock mode) | `exit=force`<br>`exit=1`<br>`exit=true`

The pipe interface includes automatic recovery from disconnections and errors, making it suitable for integration with other applications or scripts.

> :round_pushpin: _Note: cmatrix-pipes is probably not particularly portable or efficient, but it won't hog
**too** much CPU time._

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :camera: Captures

#### :small_blue_diamond: Screenshots

<!-- ![Special Font & bold](data/img/capture_bold_font.png?raw=true "cmatrix -bx") -->
<p align="center">
<img src="./data/img/capture_bold_font.png" alt="cmatrix screenshot">
</p>

#### :small_blue_diamond: Screencasts

<!-- ![Movie-Like Cast](data/img/capture_orig.gif?raw=true "cmatrix -xba") -->
<p align="center">
<img src="./data/img/capture_orig.gif" alt="cmatrix screencast">
</p>

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :link: Original Project
CMatrix-Pipes is a fork of [CMatrix](https://github.com/abishekvashok/cmatrix), originally created by **Chris Allegretta** and maintained by **Abishek V Ashok**. All credit for the original CMatrix goes to them and the many contributors to that project.

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :book: Contribution Guide
CMatrix-Pipes is focused specifically on the named pipe control feature. When contributing, please keep in mind:

- **Named Pipe Features**: Contributions related to the named pipe control interface and its functionality are welcome here.
- **Core CMatrix Features**: For bug fixes, improvements, or features related to the core CMatrix functionality (not related to pipes), please submit them to the [original CMatrix project](https://github.com/abishekvashok/cmatrix) instead.
- **Issue Labeling**: When opening issues, please label them appropriately to indicate whether they are pipe-related or core features.
- **Pull Requests**: For pipe-related improvements, please open a PR with detailed descriptions of what you've changed and why.

For additional contribution guidelines, see [contributing.md](./CONTRIBUTING.md).

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :page_facing_up: License
This software is provided under the GNU GPL v3. [View License](./COPYING)