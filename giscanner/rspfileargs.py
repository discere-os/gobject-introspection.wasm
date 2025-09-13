# -*- Mode: Python -*-
# Copyright (c) 2025 L. E. Segovia <amy@centricular.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

import os
import sys


def get_rspfile_args(rspfile):
    '''
    Response files are useful on Windows where there is a command-line character
    limit of 8191 because when passing sources as arguments to g-ir-scanner this
    limit can be exceeded in large codebases.

    There is no specification for response files and each tool that supports it
    generally writes them out in slightly different ways, but some sources are:
    https://docs.microsoft.com/en-us/visualstudio/msbuild/msbuild-response-files
    https://docs.microsoft.com/en-us/windows/desktop/midl/the-response-file-command
    '''
    import shlex
    if not os.path.isfile(rspfile):
        sys.exit('Response file {!r} does not exist'.format(rspfile))
    try:
        with open(rspfile, 'r') as f:
            cmdline = f.read()
    except OSError as e:
        sys.exit('Response file {!r} could not be read: {}'
                 .format(rspfile, e.strerror))
    return shlex.split(cmdline)
