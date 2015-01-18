#!/bin/sh
set -e
autoreconf --install --force --warnings=all
sh ./default-configure
