# martrix
martrix is an ArtNet/sACN RGB matrix grid visualizer

It can be used to plan shows or to create an interesting fixture using a
standard computer projector. Data input is accepted via ArtNet or sACN,
either of which is output by most lighting desks and software.

## Build prerequisites

martrix uses the X11 Xrender extension and thus requires the following packages:

* libx11-dev
* libxrender-dev
* A C compiler

## Building

Running `make` on a system with the prerequisites mentioned above should build the
`martrix` binary.

## Configuration

xmartrix uses a configuration file to set up its parameters. The format is
very similar to the popular *ini* file format. An example can be found in
`martrix.cfg`.

A configuration file may consist of the following sections:

* One `[martrix]` section, containing
	* `bindhost = <bindhost>`: The interface address to listen for input data
	* `input = <type>`: Can either be `artnet` or `sacn`, default is `artnet`
	* `dimensions = <x>x<y>`: The dimensions of the visualizer grid
	* `net = <artnet net id>`: The ArtNet network number to listen for, ignored with sACN
	* `window = <x>x<y>`: Dimensions of the window to create - fullscreen if omitted
* One or more `[fixture <name>]` sections, specifying a fixture type with:
	* `channels = <num channels>`: Specify the number of channels this fixture uses
	* Any number of `<channel type> = <channel offset>` lines, where *channel type* is one of
		* `dimmer` : Color multiplier (always full if none is specified for a type)
		* `red` : Red color component
		* `green` : Green color component
		* `blue` : Blue color component
		* `white` : Value for all three color components
		* `strobe` : Currently unused
		* `uv` : Currently unused

* One `[map]` section, containing lines of the format
	* `<x>x<y> = <type name> <universe> <address>`, setting up the matrix contents

Lines starting with a semicolon are treated as comments and ignored. There is currently no
support for inline comments.

## Usage

Run `./martrix` to start the visualizer. The first and only accepted parameter
specifies a configuration file to read. If none is given, `martrix.cfg` is read
by default.

