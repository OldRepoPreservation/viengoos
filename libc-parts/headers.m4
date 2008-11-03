# Copyright (C) 2008 Free Software Foundation, Inc.
# 
# This file is part of the GNU Hurd.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_CONFIG_COMMANDS_POST([
  mkdir -p sysroot/lib libc-parts &&
  ln -sf ../../libc-parts/libc-parts.a sysroot/lib/ &&
  echo '/* This file intentionally left blank.  */' >libc-parts/libc-parts.a
])
