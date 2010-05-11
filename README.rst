=====
UnDBX
=====

`UnDBX`_ - Tool to extract, recover and undelete e-mail messages from
Outlook Express ``.dbx`` files.

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

You should end up with several files/icons in that folder:

- the **UnDBX** executable ``undbx.exe``
- the **UnDBX** HTML application ``undbx.hta``, which serves as its
  graphical user interface
- this ``README`` in HTML format
- the GPL v3 license (``COPYING``)

USAGE
-----

Just double-click either of the ``undbx`` icons that you've extracted
from the distribution ``.zip`` file. A simple dialog box should
appear.

If you hit the *Extract!* button in the dialog box, **UnDBX** will
extract e-mail messages (as individual ``.eml`` files), from all of
the ``.dbx`` files in the Outlook Express storage folder, to
corresponding sub-folders on the Desktop.

The same dialog box also lets you browse and select other input and
output folders, and optionally enable recovery mode for corrupted
``.dbx`` files.

OPERATION
---------

When run for the first time, **UnDBX** extracts all the messages as
individual ``.eml`` files.

In subsequent runs **UnDBX** *synchronizes* the output folder with the
contents of the ``.dbx`` file:

- new messages in the ``.dbx`` file, that do not correspond to
  existing ``.eml`` files, will be extracted
- existing ``.eml`` files, that do not correspond to messages in the
  ``.dbx`` file, will be treated as deleted messages and will be
  deleted

This way **UnDBX** can facilitate *fast* incremental backup of
``.dbx`` files.

The file names of extracted ``.eml`` files are composed from the
contents of the ``From:``, ``To:`` and ``Subject:`` message
headers. The modification time of each file is set to match the date
specified by the ``Date:`` header.

In recovery mode, **UnDBX** can open corrupted ``.dbx`` files
(including files larger than 2GB) and recover their contents and
undelete deleted messages.


ADVANCED USAGE
---------------

NORMAL OPERATION
~~~~~~~~~~~~~~~~

Run the following command in a command-line window, to extract e-mail
messages from all the ``.dbx`` files in ``<DBX-FOLDER>`` to
corresponding sub-folders in ``<OUTPUT-FOLDER>``:

::

    undbx <DBX-FOLDER> <OUTPUT-FOLDER>

You may also specify a single ``.dbx`` file to extract, instead of a
folder.

If the destination folder is omitted, the ``.dbx`` files will be
extracted to sub-folders in the current working folder.

RECOVERY MODE
~~~~~~~~~~~~~

In recovery mode, **UnDBX** attempts to recover e-mail messages from
corrupted ``.dbx`` files:

::

    undbx --recover <DBX-FOLDER> <OUTPUT-FOLDER>

Again, you may omit the output folder, and you may specify a single
``.dbx`` file, instead of an input folder.

During the recovery process **UnDBX** scans the ``.dbx`` file for
e-mail message fragments, and collects them into ``.eml``
files. Recovery can take a long while to complete, so please be
patient.

In recovery mode **UnDBX** always extracts all the messages it can
find, instead of extracting only those messages that have not been
extracted yet.

Keep in mind that recovered messages may be corrupted.

DELETED MESSAGES
~~~~~~~~~~~~~~~~

Deleted messages are normally stored by Outlook Express in a special
folder (e.g. "Deleted Items"), and can thus be restored by extracting
them from the corresponding ``.dbx`` file.

Most of the contents of deleted messages still exists in the original
``.dbx`` file, as long it has not been compacted or otherwise
modified. 

In recovery mode, **UnDBX** attempts to undelete any deleted message
fragment it can find.

NOTE: Outlook Express overwrites the first 4 bytes of every 512-byte
chunk of each deleted message. In other words, some data is
permanently lost when a message is deleted and *there's no general way
to fully recover deleted messages*.


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

    tar -xvzf undbx-0.20.tar.gz
    cd undbx-0.20
    ./configure
    make
    make install

On Windows, this means that you need to install either `Cygwin`_ or
`MinGW`_.

If you got the source code from the source repository, you'll need to
generate the ``configure`` script before building **UnDBX**, by
running

::

    autoreconf -vfi


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
DBX to MBOX Converter, Copyright (C) 2008, 2009 Ulrich Krebs
<ukrebs@freenet.de>

**UnDBX** contains `RFC-2822`_, and `RFC-2047`_ parsing code that was
adapted from `GNU Mailutils`_ - a suite of utilities for electronic
mail, Copyright (C) 2002, 2003, 2004, 2005, 2006, 2009, 2010 Free
Software Foundation, Inc.

.. _DbxConv: http://www.ukrebs-software.de/english/dbxconv/dbxconv.html
.. _RFC-2822: http://www.faqs.org/rfcs/rfc2822
.. _RFC-2047: http://www.faqs.org/rfcs/rfc2047
.. _GNU Mailutils: http://www.gnu.org/software/mailutils/

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
along with this program. If not, see
`<http://www.gnu.org/licenses/>`_.


