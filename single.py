#!/usr/bin/env python

import argparse
import os, sys
import re
import io
import datetime as dt

# python 3 compatibility
try:
    import cStringIO as sstream
except ImportError:
    sstream = io

intro = """// The MIT License (MIT)

// Copyright (c) {time.year} Danny Y.

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// This file was generated with a script.
// Generated {time} UTC
// This header is part of {project} (revision {revision})
// {repository}

#ifndef {guard}
#define {guard}

"""

class SingleFileGenerator(object):
    def __init__(self, config, project, repository):
        self.includes = set()
        self.project = project
        self.config = config
        self.include = re.compile('#include <{0}/(.*?)>'.format(project))
        self.fp = sstream.StringIO()
        self.include_guard = project.upper() + '_SINGLE_INCLUDE_HPP'
        revision, version = self.get_revision(), None

        self.fp.write(intro.format(time=dt.datetime.utcnow(), project=project, repository=repository,
                                   revision=revision, version=version, guard=self.include_guard))

        if not config.quiet:
            print('Creating single header for project {}.'.format(project))
            print('Current revision: {revision}\n'.format(revision=revision))


    def is_include_guard(self, line):
        return line == '#pragma once'

    def get_revision(self):
        return os.popen('git rev-parse --short HEAD').read().strip()

    def get_version(self):
        return os.popen('git describe --tags --abbrev=0').read().strip()

    def get_include(self, line):
        match = self.include.match(line)
        if match:
            # local include found
            full_path = os.path.join(self.project, match.group(1))
            return full_path

        return None

    def process_file(self, filename):
        normalized_filename = os.path.abspath(os.path.join(self.base_dir, filename))
        filename = filename.replace('\\', '/')
        if normalized_filename in self.includes:
            return

        self.includes.add(normalized_filename)

        if not self.config.quiet:
            print('processing {}'.format(normalized_filename))

        self.fp.write('// beginning of {}\n\n'.format(filename))
        last_line_comment = False

        with io.open(normalized_filename, 'r', encoding='utf-8') as f:
            for line in f:
                # skip comments
                if line.startswith('//'):
                    last_line_comment = True
                    continue

                stripped = line.strip()

                # skip include guard non-sense
                if self.is_include_guard(stripped):
                    continue

                # see if it's an include file
                name = self.get_include(stripped)

                if name:
                    self.process_file(name)
                    continue

                if last_line_comment and not stripped:
                    # empty line, skip
                    continue

                # line is fine
                self.fp.write(line)
                last_line_comment = False

        self.fp.write('// end of {}\n\n'.format(filename))

    def get_string(self):
        self.fp.write('#endif // {}\n'.format(self.include_guard))
        return self.fp.getvalue()

    def change_directory(self, directory):
        script_path = os.path.dirname(os.path.realpath(__file__))
        self.working_dir = os.getcwd()
        self.base_dir = os.path.join(script_path, directory)

    def write_to_file(self):
        module_file = self.project + '.hpp'
        output_path = '.'

        if self.config.output:
            output_path = os.path.normpath(os.path.join(self.working_dir, self.config.output))
            if not os.path.exists(output_path):
                os.makedirs(output_path)

        output_file = os.path.join(output_path, module_file)
        write_mode = 'w' if not self.config.lf_only else 'wb'

        with io.open(output_file, write_mode) as f:
            f.write(self.get_string())

        print('Successfully output file to ', output_file)


def main():
    description = "Converts the project to a single file for convenience."
    # command line parser
    parser = argparse.ArgumentParser(usage='%(prog)s [options...] module', description=description)
    # parser.add_argument('module', help='module to convert to single file', metavar='module')
    parser.add_argument('--output', '-o', help='output directory to place file in', metavar='dir')
    parser.add_argument('--quiet', help='suppress all output', action='store_true')
    # parser.add_argument('--strip', '-s', help='strip all doc comments from output', action='store_true')
    parser.add_argument('--lf-only', help='writes LF instead of CRLF on Windows', action='store_true')
    args = parser.parse_args()

    generator = SingleFileGenerator(args, 'sqlitexx', 'https://github.com/Rapptz/sqlitexx')
    generator.change_directory('include')
    generator.process_file('sqlitexx/connection.hpp')
    generator.write_to_file()

if __name__ == '__main__':
    main()
