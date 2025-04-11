#!/usr/bin/env python3
import sys
import glob
import subprocess
import os

# Get the directory and construct the patch file pattern
directory = sys.argv[1]
patch_pattern = f"{directory}/*.patch"

print("HELLO", patch_pattern)

# Find patch files matching the pattern
patches = glob.glob(patch_pattern)

print("HELLO2")
