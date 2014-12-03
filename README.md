$Id$

Portable File-system Libraries

    See COPYRIGHT-GPL for license information.

Download instructions

    Grab the main 'projects' component:

	$ git clone https://github.com/pscedu/proj.git
	$ cd proj

    Next, select which components to fetch by manually enabling/
    disabling repositories in `tools/update.sh'.  Then run this script.
    By default, all components are fetched:

	$ ./tools/update

Configuration instructions

    Similiar to GNU autoconfig `./configure', the PFL build
    infrastructure probes the host machine for API compatibility.

    It stores all such information in
    `mk/gen-localdefs-$HOSTNAME-pickle.mk'.  The build infrastructure
    uses heuristics to determine when this probe is out of date and
    regathers it automatically.  If for some reason it needs to be
    regathered, manually deleting this file will cause the build
    infrastructure to regenerate it.

	File		Description
	-------------------------------------------------
	mk/defs.mk	default configuration settings
	mk/main.mk	core building rules
	mk/local.mk	(optional) custom/override settings

    Any custom settings should go in `mk/local.mk'.  Modification of the
    other files listed above is generally not necessary.

Installation instructions

    After configuration, building of PFL and any components therein
    selected follows a standard make build procedure:

	$ make
	$ sudo make install

Custom components

    Often is is useful to add additional components/repositories to stay
    in sync with any updates to PFL e.g. local deployment configuration.
    This can be achieved by placing the component in the following
    location:

	inf/$name

    Next, an `update.sh' file needs to be added.  See examples for
    details.