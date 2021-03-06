|buildstatus|_
|codecov|_
|nala|_

Monolinux C library
===================

Features:

- Inter-thread message passing.

- Logging.

- A simple shell.

- Timers.

- Network management.

- DHCP and NTP clients.

- RTC management.

- Device-mapper verity target.

- 1-wire (without sysfs).

Requires POSIX and currently Linux.

Used by the `Monolinux`_ project.

Development environment installation
====================================

.. code-block:: shell

   $ sudo apt install gcovr
   $ sudo pip3 install pyinotify nala

Unit testing
============

Source the development environment setup script.

.. code-block:: shell

   $ source setup.sh

Execute all unit tests.

.. code-block:: shell

   $ make -s -j4
   ...

Execute one test suite.

.. code-block:: shell

   $ make -s -j4 ARGS=bus
   ...

.. |buildstatus| image:: https://travis-ci.org/eerimoq/monolinux-c-library.svg
.. _buildstatus: https://travis-ci.org/eerimoq/monolinux-c-library

.. |codecov| image:: https://codecov.io/gh/eerimoq/monolinux-c-library/branch/master/graph/badge.svg
.. _codecov: https://codecov.io/gh/eerimoq/monolinux-c-library

.. |nala| image:: https://img.shields.io/badge/nala-test-blue.svg
.. _nala: https://github.com/eerimoq/nala

.. _Monolinux: https://github.com/eerimoq/monolinux
