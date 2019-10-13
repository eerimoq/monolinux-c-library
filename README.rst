|buildstatus|_
|codecov|_

Monolinux C library
===================

The Monolinux C library.

Installation
============

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

Automatically build and run a test suite when one of its files has
been modified.

.. code-block:: text

   $ cd ml/tst/shell
   $ ml test
   ...
   ============================================================
   CC main.c
   LD /home/erik/workspace/monolinux/ml/tst/shell/build/suite

   Running tests...

        1 - 6 |  ......

   Test results:

       PASS various_commands (50.85ms)
       PASS ls (50.77ms)
       PASS cat (50.95ms)
       PASS command_editing (50.90ms)
       PASS quotes (50.73ms)
       PASS history (50.62ms)

   Tests: 6 passed, 6 total
   Time:  352.57ms
   ============================================================
   CC main.c
   LD /home/erik/workspace/monolinux/ml/tst/shell/build/suite

   Running tests...

   <more output>

File tree
=========

This is the file tree of the Monolinux C library repository.

.. code-block:: text

   monolinux-c-library/         - this repository
   ├── bin/                     - executables
   ├── LICENSE                  - license
   ├── make/                    - build system
   ├── ml/                      - the Monolinux C library
   └── setup.sh                 - development environment setup script

.. |buildstatus| image:: https://travis-ci.org/eerimoq/monolinux-c-library.svg
.. _buildstatus: https://travis-ci.org/eerimoq/monolinux-c-library

.. |codecov| image:: https://codecov.io/gh/eerimoq/monolinux-c-library/branch/master/graph/badge.svg
.. _codecov: https://codecov.io/gh/eerimoq/monolinux-c-library
