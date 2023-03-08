import os
import sys
import json
import pickle

# list of extensions to bundle
extensions = ['parquet', 'icu', 'json']

# path to target
basedir = os.getcwd()
target_dir = os.path.join(basedir, 'src', 'duckdb')
gyp_in = os.path.join(basedir, 'binding.gyp.in')
gyp_out = os.path.join(basedir, 'binding.gyp')
cache_file = os.path.join(basedir, 'filelist.cache')

# path to package_build.py
os.chdir(os.path.join('..', '..'))
scripts_dir = 'scripts'

sys.path.append(scripts_dir)
import package_build

defines = ['BUILD_{}_EXTENSION'.format(ext.upper()) for ext in extensions]

# fresh build - copy over all of the files
(source_list, include_list, original_sources) = package_build.build_package(target_dir, extensions, False)

# # the list of all source files (.cpp files) that have been copied into the `duckdb_source_copy` directory
# print(source_list)
# # the list of all include files
# print(include_list)
source_list = [os.path.relpath(x, basedir) if os.path.isabs(x) else os.path.join('src', x) for x in source_list]
include_list = [os.path.join('src', 'duckdb', x) for x in include_list]
libraries = []
windows_options = ['/GR']
cflags = ['-frtti','-v']

def sanitize_path(x):
    return x.replace('\\', '/')

source_list = [sanitize_path(x) for x in source_list]
include_list = [sanitize_path(x) for x in include_list]
libraries = [sanitize_path(x) for x in libraries]

with open(gyp_in, 'r') as f:
    input_json = json.load(f)

def replace_entries(node, replacement_map):
    if type(node) == type([]):
        for key in replacement_map.keys():
            if key in node:
                node.remove(key)
                node += replacement_map[key]
        for entry in node:
            if type(entry) == type([]) or type(entry) == type({}):
                replace_entries(entry, replacement_map)
    if type(node) == type({}):
        for key in node.keys():
            replace_entries(node[key], replacement_map)


replacement_map = {}
replacement_map['${SOURCE_FILES}'] = source_list
replacement_map['${INCLUDE_FILES}'] = include_list
replacement_map['${DEFINES}'] = defines
replacement_map['${LIBRARY_FILES}'] = libraries
replacement_map['${CFLAGS}'] = cflags
replacement_map['${WINDOWS_OPTIONS}'] = windows_options

replace_entries(input_json, replacement_map)

with open(gyp_out, 'w+') as f:
    json.dump(input_json, f, indent=4, separators=(", ", ": "))
