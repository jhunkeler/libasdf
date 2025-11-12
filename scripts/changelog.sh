#!/bin/sh
# Run towncrier to update changelog--this script is called automatically when
# running bumpver
towncrier build --yes
