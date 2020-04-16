# Deepin screenshot - Area select

## Description

A screen area selection tool extracted from the deepin-screenshot tool

## Dependencies (same as deepin-screenshot)

- Qt (>=5.6),
- debhelper (>=9),
- cmake, qt5-default, qtbase5-dev, pkg-config, libqt5svg5-dev, libqt5x11extras5-dev, qttools5-dev-tools,
- libxcb-util0-dev, libstartup-notification0-dev,
- qtbase5-private-dev, qtmultimedia5-dev, x11proto-xext-dev, libmtdev-dev, libegl1-mesa-dev, x11proto-record-dev,libxtst-dev,
- libudev-dev, libfontconfig1-dev, libfreetype6-dev, libglib2.0-dev, libxrender-dev,
- libdtkwidget-dev, libdtkwm-dev, deepin-gettext-tools

## Installation

- first, install the dependencies rightly;
- secord, make a directory: build; run `cmake ../`; `make`; `make install`

## Usage
Run the command: `deepin-screenshot`

## License

deepin-screenshot is licensed under [GPLv3](LICENSE).
