import os
import os.path
import re
import fnmatch
import subprocess

if __name__ == "__main__":

    includes = ['*.cpp', '*.h'] # for files only
    excludes = ['.git','python','opengl'] # for dirs and files

    FNULL = open(os.devnull, 'w')    #use this if you want to suppress output to stdout from the subprocess
    
    # transform glob patterns to regular expressions
    includes = r'|'.join([fnmatch.translate(x) for x in includes])
    #excludes = r'|'.join([fnmatch.translate(x) for x in excludes]) or r'$.'

    try:
        for root, dirs, files in os.walk(".",topdown=True):
            #usuwanie folderow z excludes
            #for d in dirs:
            #    if d in excludes:
            #        dirs.remove(d)
            dirs[:] = [d for d in dirs if d not in excludes]

            files = [os.path.join(root, f) for f in files]
            files = [f for f in files if re.match(includes, f)]


            for f in files:
                print(f)
                command = "clang-format.exe -style=file -i " + f
                subprocess.Popen(command, stdout=subprocess.PIPE,
                         stderr=None, stdin=subprocess.PIPE)
                #stdout, stderr = p.communicate()
                pass
    except(StopIteration):
        pass
    pass