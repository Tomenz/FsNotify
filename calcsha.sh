#!/bin/bash

Filename=$(basename $1)
ln $1 /tmp/backup/$Filename
echo "$(date) - $(sha1sum /tmp/backup/$Filename)" >> ../checksums
rm /tmp/backup/$Filename
