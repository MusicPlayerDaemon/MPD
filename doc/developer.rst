Developer's Manual
##################

Introduction
************

This is a guide for those who wish to hack on the MPD source code.  MPD is an open project, and we are always happy about contributions.  So far, more than 150 people have contributed patches. This document is work in progress.  Most of it may be incomplete yet.  Please help!

Code Style
**********

* indent with tabs (width 8)
* don't write CPP when you can write C++: use inline functions and constexpr instead of macros
* comment your code, document your APIs
* the code should be C++14 compliant, and must compile with :program:`GCC` 6.0 and :program:`clang` 3.4
* report error conditions with C++ exceptions, preferable derived from :envvar:`std::runtime_error`
* all code must be exception-safe
* classes and functions names use CamelCase; variables are lower-case with words separated by underscore

Some example code:

.. code-block:: c

    Foo(const char *abc, int xyz)
    {
        if (abc == nullptr) {
                LogWarning("Foo happened!");
                return -1;
        }

        return xyz;
    }

Hacking The Source
******************

MPD sources are managed in a git repository on
`Github <https://github.com/MusicPlayerDaemon/>`_.

Always write your code against the latest git:

.. code-block:: none

    git clone git://github.com/MusicPlayerDaemon/MPD

If you already have a clone, update it:

.. code-block:: none

    git pull --rebase git://github.com/MusicPlayerDaemon/MPD master

You can do without :code:`--rebase`, but we recommend that you rebase
your repository on the "master" repository all the time.

Configure with the option :code:`--werror`.  Enable as many plugins as
possible, to be sure that you don't break any disabled code.

Don't mix several changes in one single patch.  Create a separate patch for every change. Tools like :program:`stgit` help you with that. This way, we can review your patches more easily, and we can pick the patches we like most first.

Basic stgit usage
=================

stgit allows you to create a set of patches and refine all of them: you can go back to any patch at any time, and re-edit it (both the code and the commit message). You can reorder patches and insert new patches at any position. It encourages creating separate patches for tiny changes.

stgit needs to be initialized on a git repository:

.. code-block:: sh

    stg init

Before you edit the code, create a patch:

.. code-block:: sh

    stg new my-patch-name

stgit now asks you for the commit message.

Now edit the code. Once you're finished, you have to "refresh" the patch, i.e. your edits are incorporated into the patch you have created:

.. code-block:: sh

    stg refresh

You may now continue editing the same patch, and refresh it as often as you like. Create more patches, edit and refresh them.

To view the list of patches, type stg series. To go back to a specific patch, type stg goto my-patch-name; now you can re-edit it (don't forget stg refresh when you're finished with that patch).

When the whole patch series is finished, convert stgit patches to git commits:

.. code-block:: sh

    stg commit

Submitting Patches
******************

Submit pull requests on GitHub:
https://github.com/MusicPlayerDaemon/MPD/pulls
