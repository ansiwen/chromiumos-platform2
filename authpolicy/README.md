Authpolicy
==========

This directory contains the Authpolicy service which provides functionality to
join Active Directory (AD) domains, authenticate users against AD and to fetch
device and user policies from AD in its native GPO format, convert them into
protobufs and make them available to Chrome OS by injecting them into session
manager.

Coding conventions
------------------

Please use clang-format before commiting changes, e.g. by running
```bash
CHROMIUM_BUILDTOOLS_PATH=~/chromium/src/buildtools git clang-format --style=file HEAD^
```
before `repo upload` for single-commit uploads.
