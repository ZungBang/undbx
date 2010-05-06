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

USAGE
-----

Just double-click either of the ``undbx`` icons in the folder where
the distribution ``.zip`` file was extracted to.

You'll be presented with a dialog box where you can hit an *Extract!*
button to extract e-mail messages (as individual ``.eml`` files), from
all of the ``.dbx`` files in the Outlook Express storage folder, to
corresponding sub-folders on the Desktop.

The dialog box also lets you browse and select other input and output
folders, and optionally enable recovery mode for corrupted ``.dbx``
files.

OPERATION
---------

When run for the first time, **UnDBX** extracts all the messages as
individual ``.eml`` files.

In subsequent runs **UnDBX** *synchronizes* the output folder with the
contents of the ``.dbx`` file:

- new messages in the ``.dbx`` file, that do not correspond to
  ``.eml`` files, will be extracted from the ``.dbx`` file
- old ``.eml`` files, that do not correspond to messages in the
  ``.dbx`` file, will be treated as deleted messages and will be
  deleted

**UnDBX** was written to facilitate *fast* incremental backup of
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

Run the following command in a command shell, to extract e-mail
messages from all the ``.dbx`` files in ``<DBX-FOLDER>`` to
corresponding sub-folders in ``<OUTPUT-FOLDER>``:

::

    undbx <DBX-FOLDER> <OUTPUT-FOLDER>

You can also specify a single ``.dbx`` file to extract, instead of a
folder.

If the destination folder is omitted, the ``.dbx`` files will be
extracted to sub-folders in the current folder.

RECOVERY MODE
~~~~~~~~~~~~~

In recovery mode **UnDBX** can analyze and then recover ``.eml``
messages from corrupted ``.dbx`` files:

::

    undbx --recover <DBX-FOLDER> <OUTPUT-FOLDER>

Again, you can omit the output folder, and you can specify a single
``.dbx`` file, instead of an input folder.

Recovery mode can be rather slow and it cannot be used incrementally ,
i.e. **UnDBX** always extracts all the messages it can find, instead
of just those that have not been extracted yet.

Keep in mind that recovered messages may be corrupted. YMMV.

DELETED MESSAGES
~~~~~~~~~~~~~~~~

Deleted messages are normally stored by Outlook Express in a special
folder (e.g. "Deleted Items"), and can thus be restored by extracting
the corresponding ``.dbx`` file.

As long as the original ``.dbx`` file, from which a message has been
deleted, has not been compacted or otherwise altered, it still
contains most of the deleted message's contents.

In recovery mode, **UnDBX** attempts to undelete any deleted message
fragment it can find.

Note, however, that the first 4 bytes of every 512-byte chunk of each
deleted message are permanently lost, when the message is deleted. In
other words, there's no general way to fully recover deleted messages.


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

    ./autoreconf -vfi


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


