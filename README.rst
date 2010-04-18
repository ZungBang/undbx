=====
UnDBX
=====

`UnDBX`_ - Tool to extract e-mail messages from Outlook Express
``.dbx`` files.

.. _UnDBX: http://code.google.com/p/undbx/

Copyright (C) 2008-2010 Avi Rozen <avi.rozen@gmail.com>

DOWNLOAD
--------

Windows binary and source code archives are available at
`<http://code.google.com/p/undbx/downloads/list>`_ together with
corresponding ``sha1sum`` digests, which you can use to verify each
archive's integrity. 

INSTALLATION
------------

There's no installer or setup program. Simply extract the distribution
``.zip`` file to some folder.

USAGE
-----

Just double-click either ``undbx`` or ``undbx-launcher`` in the folder
where the distribution ``.zip`` file was extracted to.

You'll be presented twice with a standard folder selection dialog in
order to select both the input and output folders, and then **UnDBX**
will be launched to extract the ``.dbx`` files.

The input folder holds all the ``.dbx`` files to be processed. This
will typically be your Outlook Express message store folder (please
consult Microsoft Knowledge Base article `Q270670`_ for an explantion
on how to find the location of this folder).

The output folder is where the e-mail messeges will be extracted
to. **UnDBX** will create a sub-folder under the output directory for
each ``.dbx`` file found in the input directory, to hold the extracted
messages.

.. _Q270670: http://support.microsoft.com/kb/270670

ADAVNACED USAGE
---------------

Run the following command in a command shell, to extract e-mail
messages from all the ``.dbx`` files in ``<DBX-DIRECTORY>`` to
corresponding sub-directories in ``<OUTPUT-DIRECTORY>``:

::

    undbx <DBX-DIRECTORY> <OUTPUT-DIRECTORY>

You can also specify a single ``.dbx`` file to extract instead of a
directory.

If the destination directory is omitted, the ``.dbx`` files will be
extracted to sub-directories in the corrent directory.

OPERATION
---------

On first invocation all messages will be extracted as individual
``.eml`` files. Subsequent invocations will only *update* the output
directory. Any ``.eml`` files that do not correspond to messages in
the ``.dbx`` file will be treated as deleted messages and will be
deleted from the disk. Furthermore, only new messages, that do not
correspond to files on disk, will be extracted from the ``.dbx`` file.

**UnDBX** was written to facilitate *fast* incremental backup of
``.dbx`` files.

**UnDBX** can open corrupted ``.dbx`` files and ``.dbx`` files that
are larger than 2GB. Depending on how bad the damage is, **UnDBX** may
or may not be able to extract messages from such files. YMMV.


SOURCE CODE
-----------

SNAPSHOT
~~~~~~~~

**UnDBX** current source code snapshot is available at
`<http://code.google.com/p/undbx/downloads/list>`_

REPOSITORY
~~~~~~~~~~

**UnDBX** development source code may be cloned from its public Git
repository at `<http://github.com/ZungBang/undbx/tree>`_

COMPILING
~~~~~~~~~

**UnDBX** uses GNU Autoconf/Automake:

::

    tar -xvzf undbx-0.14.tar.gz
    cd undbx-0.14
    ./configure
    make
    make install

On Windows, this means that you need to install either `Cygwin`_ or
`MinGW`_.

If you got the source code from the source repository, you'll need to
generate the ``configure`` script before building **UnDBX**, by
running

::

    ./autogen.sh


.. _Cygwin: http://www.cygwin.com
.. _MinGW: http://www.mingw.org

BUGS
----

Please report problems via the **UnDBX** issue tracking system:
`<http://code.google.com/p/undbx/issues/list>`_

CERTIFICATION
-------------

Well, it works on my machine :-) `[1]`_ `[2]`_

.. _[1]: http://jcooney.net/archive/2007/02/01/42999.aspx
.. _[2]: http://www.codinghorror.com/blog/archives/000818.html

CREDITS
-------

The **UnDBX** .dbx file format parsing code is based on `DbxConv`_ - a
DBX to MBOX Converter, by Ulrich Krebs <ukrebs@freenet.de>

.. _DbxConv: http://freenet-homepage.de/ukrebs/english/dbxconv.html

LICENSE
-------

**UnDBX** is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see `<http://www.gnu.org/licenses/>`_.


