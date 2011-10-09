#!/usr/bin/env python
#
# $Id$
#
# This file is part of FreeRCT.
# FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
#
from rcdlib import corner_tiles
import argparse

parser = argparse.ArgumentParser(description='Process a corner select image.')
parser.add_argument(dest='image_file', metavar='img-file', type=str,
                   help='Image file contains the corner selection sprites')
parser.add_argument('--output', dest='output', action='store',
                   metavar='rcd-file', help='Output RCD file')
args = parser.parse_args()

if args.output is None:
    if args.image_file.lower().endswidth('.png'):
        out_name = args.image_file[:-4] + '.rcd'
    else:
        out_name = args.image_file + '.rcd'
else:
    out_name = args.output


images = corner_tiles.split_image(args.image_file, -32, -33, 64, 64)
corner_tiles.write_cornerselectRCD(images, 64, 16, out_name)

