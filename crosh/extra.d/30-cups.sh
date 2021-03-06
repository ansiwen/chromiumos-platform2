# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Crosh command that is only loaded when CUPS is present. Note that
# this command is just a stopgap measure and should be dropped once
# a proper UI is formed. See http://crbug.com/593139

USAGE_cups='[--reset]'
HELP_cups='
  Reset the state of the CUPS daemon. Will stop cupsd (if running) and remove
  any local state (e.g., saved printers, jobs, etc.). For initial development
  only; will be removed in a future release.
'

cups_reset() {
  debugd CupsResetState > /dev/null
}

cmd_cups() (
  if [ $# -ne 1 ]; then
    help "incorrect number of arguments: $@"
    return 1
  fi
  case "$1" in
    "--reset")
      cups_reset
      ;;
    *)
      help "unknown option: $1"
      return 1
  esac
)
