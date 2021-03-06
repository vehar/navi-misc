#!/usr/bin/env python
"""
This is a utility for backing up parts of navi into date-coded
directories on DVD, with MD5 catalogs stored both on DVD and
on navi itself.
This uses growisofs, and assumes your burner is at /dev/dvd.

Files and/or directories that shouldn't be backed up can
be marked with a CVS-like '.backup-ignore' file. This file
gives regexes, one per line, that match directories or files
not to back up.

 Usages:

  * navi-backup store <paths>
    Archive all paths on the current DVD, without checking for
    available space or preexisting backups

  * navi-backup status <paths>
    Using the catalogs on navi, show the backup status of all files
    in the given paths

  * navi-backup auto <paths>
    Find files under the given paths that need backing up, sort
    them, and fit as many as possible onto the remaining space
    on the current DVD.

  * navi-backup verify
    Read in all catalogs on the current disc, and verify the MD5
    digests on all files contained within. This saves date-stamped
    verification records to navi's catalog showing which files,
    if any, failed verification.

  * navi-backup pending <paths>
    Show status of only those files that need to be backed up.
    They are listed in the same order 'auto' would back them up in.

  * navi-backup md5 <paths>
    Update the MD5 cache for the given paths. This is not necessary,
    but may be run ahead of time to give a speed boost later on.
    This also happens to print md5sum-like output for the files
    in question, in case you want to use navi-backup's cache
    to get a file's digest.

  * navi-backup history
    Shows a list of backup catalogs that have been generated, along
    with the date that each was last verified.

If the path list is omitted, the current directory is assumed.

-- Micah Dowty <micah@navi.cx>
"""

import time, os, sys, re, gc
import shutil, popen2, shelve


def shellEscape(s):
    return re.sub(r"([^0-9A-Za-z_\./-])", r"\\\1", s)


class CacheDir:
    """Thin abstraction around a local directory for storing cache data."""
    def __init__(self, path="~/.navi-backup"):
        self.path = os.path.expanduser(path)
        try:
            os.mkdir(self.path)
        except:
            pass

    def getFilename(self, name):
        return os.path.join(self.path, name)

    def getDict(self, name):
        return shelve.open(self.getFilename(name), protocol=-1)


def iterMD5lines(stream):
    """Given a file-like object, iterate over md5sum-style
       lines, yielding (sum, filename) pairs.
       """
    while 1:
        line = stream.readline()
        if not line:
            break
        sum, filename = line.replace("\t", " ").strip().split(" ", 1)
        yield sum.strip(), filename.lstrip()


def localMD5(path):
    """Run one md5sum on the local machine"""
    cmd = "md5sum " + shellEscape(path)
    for sum, filename in iterMD5lines(os.popen(cmd, "r")):
        if filename == path:
            return sum
    raise IOError("Error running md5sum on %r" % path)


class MD5Cache:
    """In-memory and on-disk caches for md5sums. This stores
       a mapping of filenames to (sum, size, mtime) tuples.
       On access, the size and mtime are always verified. If
       they pass, we assume the sum is still valid. If not,
       that cache entry is disregarded an the sum is recalc'ed.
       """
    def __init__(self):
        self.cache = CacheDir().getDict('md5-cache')

    def calc(self, *filenames):
        """Calculate md5sums for one or more files remotely,
           storing all returned results to the database.
           """
        if not filenames:
            return
        sys.stderr.write("Waiting on md5sum...")

        # Double-escape filenames, since they're going through our
        # local shell and the shell on the remote machine.
        cmd = "ssh navi md5sum " + " ".join([
            shellEscape(shellEscape(os.path.abspath(f))) for f in filenames])
        for sum, filename in iterMD5lines(os.popen(cmd, "r")):
            st = os.stat(filename)
            self.cache[filename] = (sum, st.st_size, st.st_mtime)

        sys.stderr.write("\r" + " "*30 + "\r")
        self.cache.sync()

    def isCacheValid(self, filename):
        """Determine if we have a valid cache entry for the given filename"""
        filename = os.path.abspath(filename)
        try:
            sum, size, mtime = self.cache[filename]
        except KeyError:
            return False
        st = os.stat(filename)
        return st.st_size == size and st.st_mtime == mtime

    def get(self, filename):
        """Get a sum from the cache, calculating if it doesn't
           exist or if the cache is stale.
           """
        filename = os.path.abspath(filename)
        if not self.isCacheValid(filename):
            self.calc(filename)
        return self.cache[filename][0]

    def getLine(self, filename):
        """Instead of just the bare md5sum, return a line
           containing the sum and the absolute filename.
           """
        filename = os.path.abspath(filename)
        return "%s\t%s" % (self.get(filename), filename)

md5cache = MD5Cache()


class BurnFailure(Exception):
    pass

class MediaInfoFailure(Exception):
    pass


class DVDReader:
    """Abstraction for reading files from a DVD, mounting it if necessary"""
    def __init__(self, device="/dev/dvd"):
        self.device = device
        self.mountpoint = self.fstabSearch(self.device)

    def fstabSearch(self, device):
        device = os.path.realpath(device)
        for line in open("/etc/fstab"):
            line = line.strip()
            if not line:
                continue
            if line[0] == '#':
                continue
            fs, mountpoint = fields = line.split()[:2]

            if os.path.realpath(fs) == device:
                return mountpoint
        raise ValueError("DVD not found in /etc/fstab")

    def getPath(self, path):
        return os.path.join(self.mountpoint, path)

    def isMounted(self):
        device = os.path.realpath(self.device)
        for line in os.popen("mount", "r"):
            dev, on, mountpoint = line.split()[:3]
            if os.path.realpath(dev) == device:
                return True
        return False

    def mount(self):
        if os.system("mount %s" % shellEscape(self.mountpoint)):
            raise IOError("Error mounting %s" % self.mountpoint)

    def unmount(self):
        # In case any files are still open that shouldn't be...
        gc.collect()
        if os.system("umount %s" % shellEscape(self.mountpoint)):
            raise IOError("Error unmounting %s" % self.mountpoint)


class DVDBurner:
    """DVD burner abstraction, using dvd+rw-tools"""
    def __init__(self, device="/dev/dvd"):
        self.device = device
        self.messageMode = None

    def iterMediaInfo(self):
        """Iterate over the key, value pairs returned by dvd+rw-mediainfo"""
        for line in os.popen("dvd+rw-mediainfo /dev/dvd", "r"):
            l = line.split(":", 1)
            if len(l) > 1:
                key = l[0].strip()
                value = l[1].strip()
                yield key, value

    def getFreeSpace(self):
        """Returns the amount of free space on the current disc, in bytes"""
        space = 0
        for key, value in self.iterMediaInfo():
            # Store the free block count of the last track we see
            if key == "Free Blocks":
                assert value.endswith("*2KB")
                blocks = int(value[:-4])
                space = blocks * 2 * 1024
        return space

    def isBlank(self):
        """Determines whether the current disc is blank"""
        for key, value in self.iterMediaInfo():
            # Store the free block count of the last track we see
            if key == "Disc status":
                if value == "appendable":
                    return False
                elif value == "blank":
                    return True
                else:
                    raise MediaInfoFailure("Disc is neither appendable nor blank, it is %r" % value)
        raise MediaInfoFailure("No disc status found")

    def _graftEscape(self, s):
        """Escape a path for use with mkisofs graft points"""
        return s.replace("\\", "\\\\").replace("=", "\\=")

    def burn(self, pathMap):
        """Burn a new session to the current disc, with the given dict
           mapping paths on the disc to paths in our filesystem.

           This invokes growisofs (and therefore mkisofs) in a subprocess,
           feeding in a list of path mappings via stdin and getting
           progress information via stderr.
           """
        if self.isBlank():
            # Record an initial session
            sessionOption = '-Z'
        else:
            # Append to the disc
            sessionOption = '-M'

        cmd = 'growisofs %s %s -R -J -D -graft-points -path-list -' % (
            sessionOption, self.device)
        subproc = popen2.Popen4(cmd)

        # Write the path mapping list and close stdin
        try:
            for disc, fs in pathMap.iteritems():
                subproc.tochild.write(self._graftEscape(disc) + "=" + self._graftEscape(fs) + "\n")
            subproc.tochild.close()
        except IOError:
            # Look for stdout/stderr output that might explain this
            self.readBurnOutput(subproc)
            raise

        self.readBurnOutput(subproc)
        retval = subproc.wait()
        if retval != 0:
            raise BurnFailure("%r failed with error code %r" % (cmd, retval))

    def readBurnOutput(self, subproc):
        """Read all the output until stdout/stderr is closed"""
        while 1:
            line = subproc.fromchild.readline()
            if not line:
                break
            self.parseBurnOutput(line.strip())

    def parseBurnOutput(self, line):
        """This receives lines of output from growisofs and tries to make sense of them"""

        # Is it a progress message?
        match = re.match(r"\s*(\d+\.\d+)\% done, estimate finish (.*)", line)
        if match:
            percent = float(match.group(1))
            finishTime = time.mktime(time.strptime(match.group(2),
                                                   "%a %b %d %H:%M:%S %Y"))
            return self.burnProgress(percent, finishTime)

        # Is it one of those damn annoying iso9660 renaming warnings
        # that we don't care about at all since we use the rockridge names?
        if re.match(r"Using [^a-z ]+ for.*", line):
            return

        self.burnMessage(line)

    def burnMessage(self, line):
        """Unparsed messages from our burn command go here"""
        self.setMessageMode("other")
        print line

    def getProgressBar(self, percent, width=50):
        """Create a textual progress bar representing some percentage completion"""
        complete = int(percent / 100.0 * width + 0.5)
        return "#"*complete + "-"*(width-complete)

    def burnProgress(self, percent, finishTime):
        """This receives burn progress updates, in the form of a percent
           completion and a projected completion time.
           """
        self.setMessageMode("progress")
        now = time.time()
        remaining = max(0, finishTime - now)
        status = "%7.2f%% %s (%d:%02d remaining)" % (
            percent, self.getProgressBar(percent), int(remaining/60),
            int(remaining)%60)
        sys.stderr.write("\r" + " "*75 + "\r" + status + " ")

    def setMessageMode(self, mode):
        """Keep track of what we're outputting, and put in a separator
           if it changes.
           """
        if mode != self.messageMode:
            if self.messageMode == "progress":
                # An extra blank line, since the progress meter didn't have a \n
                print
            print
        self.messageMode = mode


class CatalogWriter:
    """Creates catalog files from a list of paths being backed up"""
    def __init__(self, paths):
        self.paths = paths
        self.name = self._getName()
        self.temp_name = "/tmp/navi_backup_%r.catalog" % os.getpid()
        self.cd_name = os.path.join("catalog", self.name)
        self.archive_name = os.path.join(Catalog.catalogDir, self.name)

    def _getName(self):
        """Determine the name of our catalog, based on the current time and
        on the common prefix shared by all our paths.
        """
        prefix = os.path.commonprefix(self.paths).replace(os.sep, '.')
        assert prefix[0] == '.'
        return time.strftime("%Y%m%d_%H%M%S") + prefix

    def addToDisc(self, pathMap):
        """Add this catalog to a path map used for DVD mastering"""
        pathMap[self.cd_name] = self.temp_name

    def generate(self):
        """Generate the catalog in a temporary file"""
        f = open(self.temp_name, "w")
        for path in self.paths:
            self._indexTree(f, path)
        f.close()

    def archive(self):
        """Copy the catalog to navi for archiving"""
        shutil.move(self.temp_name, self.archive_name)

    def _indexTree(self, f, path):
        if os.path.isfile(path):
            self._indexFile(f, path)
        else:
            for root, dirs, files in os.walk(path):
                for name in files:
                    self._indexFile(f, os.path.join(root, name))

    def _indexFile(self, f, path):
        """Get an md5 from the cache, and make an md5sum-style line with absolute paths"""
        path = os.path.abspath(path)
        line = md5cache.getLine(path)
        print line
        f.write(line + "\n")


class IgnoreFile:
    """Abstraction for a file full of regex to filter
       out files or directories from our backup.
       """
    def __init__(self, root):
        self.regexes = []
        path = os.path.join(root, ".backup-ignore")
        if os.path.isfile(path):
            for line in open(path):
                line = line.strip()
                if line:
                    self.regexes.append(re.compile(line))

    def filter(self, list):
        deletions = []
        for item in list:
            for regex in self.regexes:
                if regex.match(item):
                    deletions.append(item)
        for item in deletions:
            list.remove(item)


def flattenBackupPaths(paths):
    """From a list of paths, generates a list of full filenames in sorted order.
       Since this should always crawl a list of files in the same way we
       would create a backup, this is where we handle ignored directories
       and checking for special files.
       """
    for path in paths:
        if os.path.isfile(path):
            yield path
        else:
            for root, dirs, files in os.walk(path):
                # Process an ignore file if we have one
                ignore = IgnoreFile(root)
                ignore.filter(dirs)
                ignore.filter(files)

                # Directories and files are always in asciibetical order
                dirs.sort()
                files.sort()

                # Generate full paths
                for name in files:
                    fullPath = os.path.join(root, name)
                    if os.path.isfile(fullPath):
                        yield fullPath


class VerificationRecord:
    """An objefct representing the results from one verification session"""
    def __init__(self, catalog, name):
        self.catalog = catalog
        self.name = name
        self.path = os.path.join(catalog.catalogDir, "verified",
                                 catalog.name, name)

    def __repr__(self):
        return "<Verification %s for Catalog %s>" % (self.name, self.catalog.name)

    def countErrors(self):
        """Return the number of errors this verification resulted in"""
        return len(open(self.path).readlines())

    def iterResults(self):
        """Iterate over all results from this verification. Returns
           (type, filename) tuples.
           """
        for line in open(self.path):
            type, filename = line.split(" ", 1)
            yield (type.strip(), filename.strip())


class Catalog:
    """An object representing one archived catalog"""
    catalogDir = "/navi/backups/catalog"

    def __init__(self, name):
        self.name = name
        self.path = os.path.join(self.catalogDir, name)

    def __repr__(self):
        return "<Catalog %s>" % self.name

    def getRawDate(self):
        """Return the date in 'YYYYMMDD_HHMMSS' format"""
        return self.name.split('.', 1)[0]

    def getDateTime(self):
        """Return the date in ('YYYY-MM-DD', 'HH:MM:SS') format"""
        return re.sub(r"(....)(..)(..)_(..)(..)(..)",
                      r"\1-\2-\3 \4:\5:\6", self.getRawDate()).split()

    def iterVerifications(self):
        """List all verification records for this catalog, sorted by date"""
        try:
            verified = os.listdir(os.path.join(self.catalogDir, "verified", self.name))
        except OSError:
            return
        verified.sort()
        for name in verified:
            yield VerificationRecord(self, name)

    def iterFiles(self):
        """Iterate over all files in this catalog. Yields (md5sum, fielname) tuples"""
        for line in open(self.path):
            sum, filename = line.replace("\t", " ").strip().split(" ", 1)
            yield (sum, filename.lstrip())

    def getLatestVerification(self):
        """Return this catalog's latest verification, or None if it hasn't
           had any verifications.
           """
        verifications = list(self.iterVerifications())
        if verifications:
            return verifications[-1]

def iterCatalogs():
    """A generator that yields all catalogs, sorted by date"""
    catalogs = os.listdir(Catalog.catalogDir)
    catalogs.sort()
    for name in catalogs:
        catalog = Catalog(name)
        if os.path.isfile(catalog.path):
            yield catalog


class CatalogIndex:
    """A searchable index of backup catalogs. This is made persistent by
       a shelve database mapping filenames to (md5, (date, time), catalogName)
       tuples. The empty string is mapped to the name of the most recent catalog
       we've indexed.
       """
    def __init__(self):
        self.cache = CacheDir().getDict('catalog-index')
        self.reindexAll()
        self.invalidations = InvalidationIndex()

    def reindexAll(self):
        """Reindex all catalog files, in order, skipping those that
           we've already stored in the index.
           """
        latest = self.cache.get('')
        i = iterCatalogs()

        # Look for the first catalog we haven't seen yet
        if latest:
            try:
                while 1:
                    catalog = i.next()
                    if catalog.name == latest:
                        break
            except StopIteration:
                # Oops, our latest catalog doesn't even exist.
                # Start over from the beginning.
                i = iterCatalogs()

        # All other catalogs need indexing
        for catalog in i:
            self.reindexCatalog(catalog)
            latest = catalog.name

        # Save a new latest catalog
        self.cache[''] = latest
        self.cache.sync()

    def reindexCatalog(self, catalog):
        print "Reading %r" % catalog
        currentDate = catalog.getDateTime()
        for sum, filename in catalog.iterFiles():
            self.cache[filename] = (sum, currentDate, catalog.name)

    def isFileModified(self, filename):
        """Check whether a file has been modified since it was archived"""
        filename = os.path.abspath(filename)
        storedSum = self.cache[filename][0]
        currentSum = md5cache.get(filename)
        return currentSum != storedSum

    def needsBackup(self, filename):
        """Return True if the given file needs a new backup"""
        filename = os.path.abspath(filename)
        try:
            if self.invalidations.isInvalid(filename, self.cache[filename][2]):
                return True

            return self.isFileModified(filename)
        except KeyError:
            return True

    def fileStatus(self, filename):
        """Show a status line for one file. This starts with a one-character
           status code: '?' if the file is unknown, 'M' if it has been modified
           since the backup, and ' ' if it matches the backup. This is followed
           by a backup timestamp if one exists, then the filename.
           """
        try:
            backupDate = self.cache[os.path.abspath(filename)][1][0]
            catalogName = self.cache[os.path.abspath(filename)][2]
        except KeyError:
            status = '?'
            backupDate = ''
        else:
            if self.invalidations.isInvalid(filename, catalogName):
                status = 'I'
            elif self.isFileModified(filename):
                status = 'M'
            else:
                status = ' '
        print "%1s %10s  %s" % (status, backupDate, filename)


class InvalidationIndex:
    """A searchable index of errors from backup verification results, made
       persistent using shelve. For every verification error, we map
       repr((filename, catalogName)) to the type of error that occurred. We
       keep track of which verification records have been processed by including
       a dict of them mapped to the empty string.
       """
    def __init__(self):
        self.cache = CacheDir().getDict('invalidation-index')
        self.reindexAll()

    def reindexAll(self):
        """Reindex all catalog files, in order, skipping those that
           we've already stored in the index.
           """
        processed = self.cache.get('')
        if not processed:
            processed = {}

        for catalog in iterCatalogs():
            for verification in catalog.iterVerifications():
                if not verification.path in processed:
                    self.reindexVerification(verification)
                    processed[verification.path] = 1

        self.cache[''] = processed
        self.cache.sync()

    def reindexVerification(self, verification):
        print "Reading %r" % verification
        for type, filename in verification.iterResults():
            filename = os.path.abspath(filename)
            self.cache[repr((filename, verification.catalog.name))] = type

    def isInvalid(self, filename, catalogName):
        """Check whether a file has been invalidated"""
        filename = os.path.abspath(filename)
        return bool(self.cache.get(repr((filename, catalogName))))


def backup(paths, burner):
    """Perform a normal catalogging and backup of a list of paths"""
    paths = map(os.path.abspath, paths)

    # Generate catalog
    cat = CatalogWriter(paths)
    cat.generate()

    # Map fs paths to disc paths
    pathMap = {}
    cat.addToDisc(pathMap)
    prefix = time.strftime("%Y%m%d")
    for path in paths:
        assert path[0] == os.sep
        pathMap[prefix + path] = path

    # Burn the DVD. If that worked, archive the catalog
    burner.burn(pathMap)
    cat.archive()
    print "\nFinished burning and archiving."
    print "Please label this disc: %r" % cat.name


class CatalogVerifier:
    """Performs verifications of one catalog file on a mounted DVD,
       by checking that all files exist and match their saved MD5sums.
       """
    def __init__(self, dvd):
        self.dvd = dvd
        self.totalFiles = 0
        self.totalFailures = 0

    def verify(self, path, catalogName):
        print 'Verifying catalog "%s"' % catalogName
        self.logBuffer = []

        # First, make sure the catalog itself matches navi
        if localMD5(path) != localMD5(os.path.join(Catalog.catalogDir, catalogName)):
            print "*** Catalog file doesn't match the copy stored on navi"
            sys.exit(1)

        for catalogSum, filename in iterMD5lines(open(path)):
            try:
                dvdPath = self.getPath(catalogName, filename)

                if os.path.isfile(dvdPath):
                    if localMD5(dvdPath) == catalogSum:
                        self.log(filename)
                    else:
                        self.log(filename, "MODIFIED")
                else:
                    self.log(filename, "MISSING")

            except KeyboardInterrupt:
                raise
            except:
                self.log(filename, "ERROR")

        # If we got this far, store a verification record
        self.saveResults(catalogName)

    def saveResults(self, catalogName):
        """Save the results of verifying 'catalogName'."""
        verifiedDir = os.path.join(Catalog.catalogDir, "verified", catalogName)
        if not os.path.isdir(verifiedDir):
            os.makedirs(verifiedDir)
        datecode = time.strftime("%Y%m%d_%H%M%S")
        logFile = os.path.join(verifiedDir, datecode)
        self.saveLog(logFile)

    def getPath(self, catalogName, filename):
        """Get the on-DVD path for the given filename. This
           figures out which date-coded directory the path
           would be inside, then uses our DVDReader to construct
           a full path.
           """
        if filename[0] == os.path.sep:
            filename = filename[1:]
        dateDir = catalogName[:8]
        return self.dvd.getPath(os.path.join(dateDir, filename))

    def writeLogEntry(self, stream, entry):
        """Write a log entry out to a file-like object"""
        stream.write("%-15s %s\n" % entry)

    def log(self, filename, problem=''):
        """Add a new log entry. It is always displayed on
           stdout, and if a problem is given it's buffered.
           """
        entry = (problem, filename)
        if problem:
            self.totalFailures += 1
            self.logBuffer.append(entry)
        self.totalFiles += 1
        self.writeLogEntry(sys.stdout, entry)

    def saveLog(self, filename):
        """Save our log out to disk"""
        f = open(filename, "w")
        for entry in self.logBuffer:
            self.writeLogEntry(f, entry)
        f.close()


def verify_disc(dvd):
    """Given a DvdReader object, verify all catalogs on the disc
       it represents and save verification records to disk.
       """
    catalogDir = dvd.getPath("catalog")
    verifier = CatalogVerifier(dvd)
    for catalog in os.listdir(catalogDir):
        path = os.path.join(catalogDir, catalog)
        if not os.path.isfile(path):
            continue
        verifier.verify(path, catalog)
    print
    print "%d files verified, %d failures." % (
        verifier.totalFiles, verifier.totalFailures)


def cmd_store(paths):
    backup(paths, DVDBurner())


def cmd_status(paths):
    index = CatalogIndex()
    for path in flattenBackupPaths(paths):
        index.fileStatus(path)


def cmd_auto(paths, assumedOverhead = 0.01, safetyTimer = 3):
    index = CatalogIndex()
    burner = DVDBurner()
    burnPaths = []

    print "Reading disc status..."
    remaining = burner.getFreeSpace()
    print "Remaining space on disc: %.1fMB" % (remaining / (1024.0 * 1024.0))
    print "Assuming %r%% overhead" % (assumedOverhead*100.0)
    remaining -= assumedOverhead * remaining
    total = 0

    for path in flattenBackupPaths(paths):
        # Show status of all files, to give visual indication of what we're scanning.
        # Only those marked with a '?' or 'M' will be backed up.
        index.fileStatus(path)

        if index.needsBackup(path):
            filesize = os.stat(path).st_size
            if filesize > remaining:
                print "Stopping, disc full"
                break

            total += filesize
            remaining -= filesize
            burnPaths.append(path)

    print "Found %d files to back up, a total of %.1fMB. About %.1fMB will be left on the disc." % (
        len(burnPaths), total / (1024.0 * 1024.0), remaining / (1024.0 * 1024.0))
    if burnPaths:
        print "Starting burn in %d seconds..." % safetyTimer
        time.sleep(safetyTimer)
        backup(burnPaths, burner)


def cmd_pending(paths):
    index = CatalogIndex()
    totalCount = 0
    totalSize = 0
    for path in flattenBackupPaths(paths):
        if index.needsBackup(path):
            index.fileStatus(path)
            totalSize += os.stat(path).st_size
            totalCount += 1

    if totalCount:
        sys.stderr.write("%d files pending backup, totalling %.1f MB\n" % (
            totalCount, totalSize / (1024.0**2)))
    else:
        sys.stderr.write("No files pending backup\n")


def cmd_md5(paths, filesPerCommand=8):
    pathGen = flattenBackupPaths(paths)
    while 1:
        # At each iteration, look for files that need md5sums until we
        # run out of source files or we hit the filesPerCommand limit.
        files = []
        try:
            while len(files) < filesPerCommand:
                file = pathGen.next()
                if md5cache.isCacheValid(file):
                    print md5cache.getLine(file)
                else:
                    files.append(file)
        except StopIteration:
            pass

        if not files:
            break
        md5cache.calc(*files)
        for file in files:
            print md5cache.getLine(file)


def cmd_verify(paths=[]):
    reader = DVDReader()
    wasMounted = reader.isMounted()
    if not wasMounted:
        reader.mount()
    try:
        verify_disc(reader)
    finally:
        if not wasMounted:
            reader.unmount()

def cmd_history(paths=[]):
    for catalog in iterCatalogs():
        verified = catalog.getLatestVerification()
        if verified:
            lastVerified = "%s (%d errors)" % (verified.name, verified.countErrors())
        else:
            lastVerified = "[Not verified]"
        print " %-30s %s" % (lastVerified, catalog.name)


def usage():
    print __doc__
    sys.exit(0)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        usage()
    command = sys.argv[1]
    paths = sys.argv[2:]
    if not paths:
        paths = '.'
    try:
        f = globals()["cmd_%s" % command]
    except:
        usage()
    else:
        f(paths)

### The End ###
