#!/usr/bin/env python
from filterlib import CommitFilter
import re, posixpath

class PythonFilter(CommitFilter):
    project = 'python'

    def readFiles(self, directory):
        # We're in an added, removed, or modified files section. Split the space-separated
        # list of files up and add them to the message, stopping when we hit a line that
        # doesn't begin with whitespace.
        while True:
            line = self.body.readline()
            if not (line.startswith('\t') or line.startswith(' ')):
                break
            line = line.strip()

            # Is this line setting the tag/branch?
            if line.startswith("Tag: "):
                self.addBranch(line.split(" ", 1)[1])
                continue

            for file in line.split(' '):
                self.addFile(posixpath.join(directory, file))

    def readLog(self):
        # Read the log message, until we hit the beginning of the diffs
        # if the message has them.
        lines = []
        while True:
            line = self.body.readline()
            if not line:
                break
            if line.startswith("Index:"):
                break
            line = line.strip()
            if line:
                lines.append(line)
        self.addLog("\n".join(lines))

    def parse(self):
        # Directory name is the second token in the subject. Only the part
        # after the first slash is the actual directory name, the first part
        # is the module.
        module, dirName = self.message['subject'].strip().split(" ")[1].split('/', 1)
        self.addModule(module)

        # Author is the first token of the from address. AFAICT all the from addresses
        # are @users.sourceforge.net, so strip that out if we have it.
        self.addAuthor(self.message['from'].split(' ')[0].replace("@users.sourceforge.net", ""))

        # Skip lines until we get to a section we can process
        while True:
            line = self.body.readline()
            if not line:
                break

            if line.endswith(' Files:\n'):
                self.readFiles(dirName)
            elif line == 'Log Message:\n':
                self.readLog()
                break

if __name__ == '__main__':
    PythonFilter().main()
