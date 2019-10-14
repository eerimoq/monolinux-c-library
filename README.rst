|buildstatus|_
|codecov|_

Monolinux C library
===================

Features:

- Inter-thread message passing.

- Logging.

- A simple shell.

- Network configuration.

- Timers.

Requires POSIX and currently Linux.

Used by the `Monolinux`_ project.

Development environment installation
====================================

.. code-block:: shell

   $ sudo apt install gcovr
   $ sudo pip3 install pyinotify narmock

Unit testing
============

Source the development environment setup script.

.. code-block:: shell

   $ source setup.sh

Execute all unit tests.

.. code-block:: shell

   $ make -s -j4
   ...

.. |buildstatus| image:: https://travis-ci.org/eerimoq/monolinux-c-library.svg
.. _buildstatus: https://travis-ci.org/eerimoq/monolinux-c-library

.. |codecov| image:: https://codecov.io/gh/eerimoq/monolinux-c-library/branch/master/graph/badge.svg
.. _codecov: https://codecov.io/gh/eerimoq/monolinux-c-library

.. _Monolinux: https://github.com/eerimoq/monolinux
