
#******************************************************************************#
# Readme:
# (1)Comments are explanations for the statement above them.
# (2)Give out path in cmd line, recursively go through the path, find out .c
#    files including the suite and tests info. Parse info out to output file.
#    Probably one .gpj file exist in the same folder with the .c file.
# (3)Coding style could be mixed(old and new).
#    Old coding style: refer "firmware"->"develop"
#                      ->"hw-integration/template/templateOnTargetUnitTest.c"
#    New coding style: refer "hal"->"develop"
#                      ->"hwvalidation/bufferManagement/bufferManagement.c"
#******************************************************************************#

import re, os, argparse
from pprint import pprint
import json

### CLASS DECLARATION ###
class CFileParse(object):
    # Parse info from one file, which specified by "filepath" and "filename"
    # Organize parsed data into dict "suites"

    def __init__(self, filepath, filename, buildfile):
        self.filepath  = filepath
        self.filename  = filename
        self.buildfile = buildfile
        self.suites    = {}
        self.fullname  = os.path.join(self.filepath, self.filename)

    def parse_suite_ptr(self):
        # suiteptr: Indicate which specific suite is called in test file.
        # Set suiteptr, coz multiple suites could exist in one file.
        # parse suite ptr
        # "suiteptrpat" applies for both old and new coding style

        suiteptrpat = r"^\s+appendUnitTestSuite\(&(\S+)\);\s*$"
        # regex format: appendUnitTestSuite(&exampleSuite_temp_2_1st);
        #               appendUnitTestSuite(&suiteptr    );
        #               applies for both old and new coding style
        for line in open(self.fullname):
            match = re.search(suiteptrpat, line)
            if(match):
                suiteptr = match.group(1)
                self.suites[suiteptr] = {"suiteptr" : suiteptr}

    def parse_suite_info(self):
        # parse suite id, suite name
        # The suite info line should match either "suiteinfo_oldpat" or
        # "suiteinfo_newpat".
        for item in self.suites.keys():
            # parse info from old coding style
            suiteinfo_oldpat = r"^\s*static unitTestSuite_t %s = \{(\d+), &\((\S+)\[\d+\]\), \"(\S+)\"\};\s*$" % (item)
            # regex format: static unitTestSuite_t exampleSuite = {100,     &(exampleTestList[0]), "Example  "};
            #               static unitTestSuite_t suiteptr     = {suiteid, &(testsptr       [0]), "suitename"};
            for line in open(self.fullname):
                match = re.search(suiteinfo_oldpat, line)
                if(match):
                    suiteid = match.group(1)
                    testsptr = match.group(2)
                    suitename = match.group(3)
                    self.suites[item]["suiteid"] = suiteid
                    self.suites[item]["testsptr"] = testsptr
                    # one suite has only one tesetptr
                    self.suites[item]["suitename"] = suitename

        for item in self.suites.keys():
            # parse info from new coding style
            suiteinfo_newpat = r"\s*initUnitTestSuite\(&%s,.*?(\d+),.*?&\((\S+)\[\d+\]\),.*?\"(\S+)\"\);" % (item)
            # regex format: initUnitTestSuite(&bufferManagementSuite, 11,      &(bufferManagementTestList[0]), "BufferManagement");
            #               initUnitTestSuite(&suiteptr,              suiteid, &(testptr                 [0]), "suitename");
            # remove '^' coz multiple line matching
            with open(self.fullname, 'r') as content_file:
                content = content_file.read()
                allmatch = re.finditer(suiteinfo_newpat, content, re.DOTALL)
                for match in allmatch:
                    suiteid = match.group(1)
                    testsptr = match.group(2)
                    suitename = match.group(3)
                    self.suites[item]["suiteid"] = suiteid
                    self.suites[item]["testsptr"] = testsptr
                    self.suites[item]["suitename"] = suitename


    def parser_tests_info(self):
        # parse tests id, name, funcptr.
        # "funcptr" defines in regex format below
        for item in self.suites.keys():
            # parse info from old coding style
            capton = False      # capture_on, flag
            self.suites[item]["tests"] = {}
            teststart_oldpat = r"^static unitTest_t %s\[\] = \{\s*$" % \
                             (self.suites[item]["testsptr"])
            # regex format: "static unitTest_t exampleTestList[] = {"
            testend_oldpat   = r"\s+\{(\d+), &(\S+), \"(\S+)\"\},\s*$"
            # regex format: "{10,     &exampleUnitTest, "HelloWorld"},"
            #               "{testid, &funcptr,         "testname"},"

            for line in open(self.fullname):
                # (1) Coz old and new coding style could exist in one file,
                # compare each line with both old and new regex patter.
                # (2) Any line cannot apply both old and new regex pat at the
                # same time, so put these two regex pat comparison together.
                # Below is to compare with old regex pattern

                match = re.search(teststart_oldpat, line)
                if(match):
                    capton = True
                    continue

                if(capton):
                    match = re.search(testend_oldpat, line)
                    if(match):
                        testid = match.group(1)
                        funcptr = match.group(2)
                        testname = match.group(3)
                        self.suites[item]["tests"][funcptr] = {}
                        self.suites[item]["tests"][funcptr]["testid"] = testid
                        self.suites[item]["tests"][funcptr]["funcptr"] = funcptr
                        self.suites[item]["tests"][funcptr]["testname"] = testname
                    else:
                        # last line format "{0, (unitTestFunctionPtr_t)0, ""}"
                        capton = False
                        continue

        for item in self.suites.keys():
            # parse info from new coding style
            if(not self.suites[item]["tests"].keys()):
                # if this item does not exist
                self.suites[item]["tests"] = {}
            testinfo_newpat  = r"\s*appendUnitTestCaseToList\(&\(%s\[\d+\]\),\s*(?:\n|\r\n|\r)*\s*\S+,.*?(\d+),.*?&(\S+),.*?\"(\S+)\"\);\s*" % (self.suites[item]["testsptr"])
            # regex format: appendUnitTestCaseToList(&(bufferManagementTestList[0]), &tcIdx, 2, &bufferManagementDdrInitUnitTest, "InitDDR");
            with open(self.fullname, 'r') as content_file:
                content = content_file.read()
                allmatch = re.finditer(testinfo_newpat, content, re.DOTALL)
                for match in allmatch:
                    testid = match.group(1)
                    funcptr = match.group(2)
                    testname = match.group(3)
                    self.suites[item]["tests"][funcptr] = {}
                    self.suites[item]["tests"][funcptr]["testid"] = testid
                    self.suites[item]["tests"][funcptr]["funcptr"] = funcptr
                    self.suites[item]["tests"][funcptr]["testname"] = testname



class CFindFiles(object):
    # find the *.c files, which includes suite declaration, in the given path
    # Append the info of each *.c file into list "cfile"
    # .c file of both old and new coding style share the same property,
    # including the same line(notified by targetlinepat below).
    def __init__(self, path):
        self.path = path        # target directory path
        self.cfile = []

    def find_C_files(self):
        targetlinepat = r"^void\s+initActiveOnTargetUnitTestSuite\s*\("
        # regex format: "void initActiveOnTargetUnitTestSuite("\
        # existed in suite test file, for both old and new coding style
        for root, dirs, files in os.walk(self.path):
            for file in files:
                # only one *.gpj file exist in one folder
                # cannot merge with for loop below, coz do not know
                # *.gpj or *.c file comes out first in the loop process
                match = re.search("\S+.gpj", file)
                if(match):
                    buildfile = file

            for file in files:
                match = re.search("\S+.c", file)
                if(match):
                    for line in open(os.path.join(root, file)):
                        match = re.search(targetlinepat, line)
                        if(match):
                            dict = {}
                            fullname = os.path.join(root, file)
                            dict[fullname] = {}
                            dict[fullname]["filepath"] = root
                            dict[fullname]["filename"] = file
                            dict[fullname]["buildfile"] = buildfile
                            self.cfile.append(dict)

def recur_parse_C(path):
    # Find all qualified *.c files, and acquire the info from each
    # *.c file by calling the func in class "CFindFiles"
    cfilefinder = CFindFiles(path)
    cfilefinder.find_C_files()
    for item in cfilefinder.cfile:
        key = item.keys()[0]
        # var "key" has no special meaning. To prevent duplicate file names,
        # so make it combined by filepath+filename
        filepath  = item[key]["filepath"]
        filename  = item[key]["filename"]
        buildfile = item[key]["buildfile"]
        cfileinfo = CFileParse(filepath, filename, buildfile)

        cfileinfo.parse_suite_ptr()
        cfileinfo.parse_suite_info()
        cfileinfo.parser_tests_info()

        item[key]["suites"] = cfileinfo.suites

    return cfilefinder

### MAIN CODE ###
if __name__ == '__main__':

    ### GET INPUT ARGS ###
    parser = argparse.ArgumentParser(description="Process cmd line args")
    parser.add_argument("--path", action="store", required=True, \
                        help="To notify the specific path where "\
                             "this script will be executed")
    parser.add_argument("--output_file_path", action="store", default=".", \
                        help="Set path for output file. Current path "\
                             "will be set as the default path.")
    parser.add_argument("--output_file_name", action="store", \
                        default="on_target_test_list.csv", \
                        help="Set name for output file. \"on_target_test_list.csv\" "\
                             "will be set as the default name.")
    args = parser.parse_args()

    ### DATA PROCESSING ###
    cfilefinder = recur_parse_C(args.path)

    ### GENERATE CSV FILE ###
    output_file = os.path.join(args.output_file_path, args.output_file_name)
    outhandle = open(output_file, 'w')
    outhandle.write("filename,suitename,suiteid,testname,testid,"\
                    "buildfile,filepath,\n")
    csvformat = "{filename},{suitename},{suiteid},{testname},{testid},"\
                "{buildfile},{filepath},\n"
    for item in cfilefinder.cfile:
        key = item.keys()[0]
        filename = item[key]["filename"]
        filepath = item[key]["filepath"]
        buildfile = item[key]["buildfile"]
        #pprint(item)
        for suiteptr in item[key]["suites"].keys():
            suitename = item[key]["suites"][suiteptr]["suitename"]
            suiteid   = item[key]["suites"][suiteptr]["suiteid"]
            for funcptr in item[key]["suites"][suiteptr]["tests"].keys():
                testname  = item[key]["suites"][suiteptr]["tests"][funcptr]["testname"]
                testid    = item[key]["suites"][suiteptr]["tests"][funcptr]["testid"]
                outhandle.write(csvformat.format(filename=filename, suitename=suitename, \
                                suiteid=suiteid, testname=testname, testid=testid, \
                                buildfile=buildfile, filepath=filepath ))

    print json.dumps(cfilefinder.cfile, sort_keys=True, indent=6)
    outhandle.close()