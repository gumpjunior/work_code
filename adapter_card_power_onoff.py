#*******************************************************************************
# Read me:
#       Two ways to enter the command: (1) Specify the commands as a command 
#       line argument with quotation marks; (2) Specify the commands in a file,
#       One command each line;
#       When sending the command to micro, each command is ending with a '\n' 
#       character. And this python program will send the command to the micro 
#       via the serial port, and then keep waiting for the data/string from the 
#       micro until a '\n' is captured.
#*******************************************************************************

import time, platform, re, sys, subprocess, os, argparse, json

git_path = subprocess.check_output(["git", "rev-parse", "--show-toplevel"])
list_git_path = git_path.split('/')
git_path = os.sep.join(list_git_path)
git_path = git_path.rstrip()
sys.path.append(git_path)
temp_path = os.path.join(git_path, "pylib")
sys.path.append(temp_path)

from pylib.standard import serial
from pylib.commonfunctions import Globals

g_os             = Globals()
g_ser            = False
g_failExit       = 1
readinstr        = ""

#***************************************************
# Command format: 0xEEcommand0xFF.
# 0xEE is the header to notify m328p the beginning 
# of a command, and 0xFF is the tail of a command.
# Meaning of each command:
# 0xEEpoweron0xFF: turn power regulator on;
# 0xEEturnOFF0xFF: turn power regulator off;
# 0xEEreadconfig0xFF: read the configuration register 
#       contents out, send from m328p to PC with 
#       MSByte first, LSByte last.
# 0xEEreadcalib0xFF: read the calibration register
#       contents out, MSByte first, LSByte last.
# 0xEEshuntvolt0xFF: to read the content of the 
#       corresponding register out, MSByte first,
#       LSByte last, convert it into decimal number
#       and multiply the precision.
# 0xEEbusvolt0xFF:   same as above
# 0xEEpower0xFF:     same as above
# 0xEEcurrent0xFF:   same as above
# 0xEEsetconfig0xFF: to set the configuration 
#       register for INA209 chip. The specific 
#       content is set in .c file.
# 0xEEsetcalib0xFF: does the same to calib reg as 
#       above.
#***************************************************

cmdSet = {

    "poweron"    : "0xEEpoweron0xFF",
    "turnOFF"    : "0xEEturnOFF0xFF",
    "readconfig" : "0xEEreadconfig0xFF",
    "readcalib"  : "0xEEreadcalib0xFF",
    "shuntvolt"  : "0xEEshuntvolt0xFF",
    "busvolt"    : "0xEEbusvolt0xFF",
    "power"      : "0xEEpower0xFF",
    "current"    : "0xEEcurrent0xFF",
    "setconfig"  : "0xEEsetconfig0xFF",
    "setcalib"   : "0xEEsetcalib0xFF",
    
}

#***************************************************
# Precision setting below due to calibration 
# configuration in micro C code.
#***************************************************

currentLSB   = 80*pow(10, -6)     # 80uA
powerLSB     = 1.6*pow(10, -3)    # 1.6mW
shuntvoltLSB = 10*pow(10, -6)     # 0.01mV, default set by INA209
busvoltLSB   = 4*pow(10, -3)      # 4mV, default set by INA209 

#***************************************************
# Parse the command line argument
# --comport: specify the COM port in use;
# --cmdsequencefile: specify the cmd sequence file,
#       path can be included for this parameter;
#***************************************************

parser = argparse.ArgumentParser(description="Process cmd line args")
parser.add_argument("--comport", action="store", required=True, \
        help="Set the COM port be used for USART communication.")
parser.add_argument("--cmdsequencefile", action="store", \
        help="Set the command sequence for microchip from specified \
        file, one command each line. File path can be included for \
        the parameter.")
parser.add_argument("--poweron", action="store_true", \
        help="Enable poweron command(0xEEpoweron0xFF) "\
        "in command sequence")
parser.add_argument("--turnOFF", action="store_true", \
        help="Enable turnOFF command(0xEEturnOFF0xFF) "\
        "in command sequence")
parser.add_argument("--readconfig", action="store_true", \
        help="Enable readconfigreg command(0xEEreadconfig0xFF) "\
        "in command sequence")
parser.add_argument("--readcalib", action="store_true", \
        help="Enable readcalib command(0xEEreadcalib0xFF) "\
        "in command sequence")
parser.add_argument("--shuntvolt", action="store_true", \
        help="Enable shuntvolt command(0xEEshuntvolt0xFF) "\
        "in command sequence")
parser.add_argument("--busvolt", action="store_true", \
        help="Enable busvolt command(0xEEbusvolt0xFF) "\
        "in command sequence")
parser.add_argument("--power", action="store_true", \
        help="Enable power command(0xEEpower0xFF) "\
        "in command sequence")
parser.add_argument("--current", action="store_true", \
        help="Enable current command(0xEEcurrent0xFF) "\
        "in command sequence")
parser.add_argument("--setconfig", action="store_true", \
        help="Enable setconfig command(0xEEsetconfig0xFF) "\
        "in command sequence")
parser.add_argument("--setcalib", action="store_true", \
        help="Enable setcalib command(0xEEsetcalib0xFF) "\
        "in command sequence")

args = parser.parse_args()

#***************************************************
# "--cmdsequencefile" and other cmd specification 
# args cannot be valid at the same time. Either  
# "--cmdsequencefile" is given or other args are
# given in the command line is fine, but not together.
# Also, cannot use try-except-else to check the cmd
# "--cmdsequencefile" has been defined or not. Coz 
# when we add this argument via "parser.add_argument", 
# this argument has been defined, but the value of 
# this argument is "None". The reason to use "False" 
# for checking is "store_true" is used in add_argument
# statement.
#***************************************************

def check_cmd_arg():

    # check any of the command defined as a cmd parameter
    if( args.poweron    != False or args.turnOFF   != False or
        args.readconfig != False or args.readcalib != False or
        args.shuntvolt  != False or args.busvolt   != False or
        args.power      != False or args.current   != False or
        args.setconfig  != False or args.setcalib  != False ):
        cmdSeqinLine = True
    else:
        cmdSeqinLine = False
    # check args.cmdsequencefile defined or not in cmd line arg
    if(args.cmdsequencefile != None):
        cmdSeqinFile = True
    else:
        cmdSeqinFile = False

    if(cmdSeqinFile and cmdSeqinLine):
        print "--cmdsequencefile and other cmd specification args "\
                "cannot be defined together in the command line."
        sys.exit(g_failExit)
    if( (not cmdSeqinFile) and (not cmdSeqinLine) ):
        print " None of the cmd specification arg is defined."
        sys.exit(g_failExit)

    cmdSeqFlag = {}
    cmdSeqFlag["cmdSeqinLine"] = cmdSeqinLine
    cmdSeqFlag["cmdSeqinFile"] = cmdSeqinFile

    return cmdSeqFlag

#***************************************************
# Parse the commands one by one in args.cmdsequenceline
# Coz they are wrapped in quotation marks, and
# separated from each other by space.
#***************************************************

def parse_cmd_from_sequence_line():

    cmdSeqinLine = []
    # The command append to the cmd sequence in order.
    # If the cmd order below is not prefered, pls change manually.
    if(args.poweron):
        cmdSeqinLine.append(cmdSet["poweron"])
    if(args.setconfig):
        cmdSeqinLine.append(cmdSet["setconfig"])
    if(args.setcalib):
        cmdSeqinLine.append(cmdSet["setcalib"])
    if(args.readconfig):
        cmdSeqinLine.append(cmdSet["readconfig"])
    if(args.readcalib):
        cmdSeqinLine.append(cmdSet["readcalib"])
    if(args.shuntvolt):
        cmdSeqinLine.append(cmdSet["shuntvolt"])
    if(args.busvolt):
        cmdSeqinLine.append(cmdSet["busvolt"])
    if(args.power):
        cmdSeqinLine.append(cmdSet["power"])
    if(args.current):
        cmdSeqinLine.append(cmdSet["current"])
    if(args.turnOFF):
        cmdSeqinLine.append(cmdSet["turnOFF"])

    return cmdSeqinLine     # return list

#***************************************************
# Parse the commands from the file;
# Value of "file" from cmd line can include filepath; 
# Comments can be included in the file starting only
# with '#';
#***************************************************

def parse_cmd_from_file(file):

    cmdSeq = []
    for line in open(file):
        line = line.lstrip()
        line = line.rstrip()
        if(line):           # line is not empty
            if( ( line[:4]== "0xEE") and (line[-4:] == "0xFF") ):
                cmdSeq.append(line)
            elif( not (line.startswith('#', 0, 1)) ):
                print line
                print "The command format is incorrect"
                sys.exit(g_failExit)

    return cmdSeq           # return list

#***************************************************
# Parse the commands, either from the command line
# arg or from the file. Return the commands in list.
#***************************************************

def acquire_cmd_sequence():

    cmdSeqFlag = check_cmd_arg()
    if(cmdSeqFlag["cmdSeqinLine"]):
        # if "--cmdsequenceline" defined
        cmdSeq = parse_cmd_from_sequence_line()
    elif(cmdSeqFlag["cmdSeqinFile"]):
        # if "--cmdsequencefile" defined
        cmdSeq = parse_cmd_from_file(args.cmdsequencefile)

    return cmdSeq           # return list

#***************************************************
# According to the OS, access the corresponding 
# serial port for communication between PC and 
# microchip.
#***************************************************

def port_setting():

    global g_ser
    # enable/set serial port according to OS
    if(g_os.LINUX or g_os.WINDOWS or g_os.CYGWIN):
        # linux   comport in format of: /dev/ttyUSB1
        # windows comport in format of: \\.\COM6
        # cygwin comport in format of: /dev/ttyS5
        g_ser = serial.Serial(args.comport, 4800)

#***************************************************
# Send the command out via the serial port from PC
# side to microchip side.
#***************************************************

def cmd_send(str_in):

    str = str_in
    for item in str:
        g_ser.write(item)

#***************************************************
# Read the data/string back from the microchip side
# to the PC side via the serial port.
# The "read_until()" will keep waiting until '\n' is
# received.
#***************************************************

def read_until():

    global g_ser, readinstr
    while(True):
        serin = g_ser.read()
        if(serin == '\n'):
            break
        readinstr += serin
    print "received by UART is:"
    print readinstr
    valuereturn = readinstr
    readinstr = ""

    return valuereturn

#*******************************************************************************
# Main Function
#*******************************************************************************

def main():

    cnt = -1
    
    port_setting()
    time.sleep(1)
    cmdSeq = acquire_cmd_sequence()

    for ipcmd in cmdSeq:
        cnt += 1
        print("command[%d] is: %s" % (cnt, ipcmd) )
        if(ipcmd == cmdSet["poweron"]):
            cmd_send(cmdSet["poweron"] + "\n")
            read_until()
        elif(ipcmd == cmdSet["turnOFF"]):
            cmd_send(cmdSet["turnOFF"] + "\n")
            read_until()
        elif(ipcmd == cmdSet["readconfig"]):
            cmd_send(cmdSet["readconfig"] + "\n")
            read_until()
        elif(ipcmd == cmdSet["readcalib"]):
            cmd_send(cmdSet["readcalib"] + "\n");
            read_until()
        elif(ipcmd == cmdSet["shuntvolt"]):
            cmd_send(cmdSet["shuntvolt"] + "\n")
            shuntvolt = read_until()
            shuntvolt = int(shuntvolt, 2)
            shuntvolt = shuntvolt *  shuntvoltLSB
            print "shuntvolt = ", shuntvolt, " V"
        elif(ipcmd == cmdSet["busvolt"]):
            cmd_send(cmdSet["busvolt"] + "\n")
            busvolt = read_until()
            busvolt = busvolt[:-3]
            busvolt = int(busvolt, 2)
            busvolt = busvolt *  busvoltLSB
            print "busvolt = ", busvolt, " V"
        elif(ipcmd == cmdSet["power"]):
            cmd_send(cmdSet["power"] + "\n")
            power = read_until()
            power = int(power, 2)
            power = power *  powerLSB
            print "power = ", power, " W"
        elif(ipcmd == cmdSet["current"]):
            cmd_send(cmdSet["current"] + "\n")
            current = read_until()
            current = int(current, 2)
            current = current *  currentLSB
            print "current = ", current, " A"
        elif(ipcmd == cmdSet["setconfig"]):
            cmd_send(cmdSet["setconfig"] + "\n")
            read_until()
        elif(ipcmd == cmdSet["setcalib"]):
            cmd_send(cmdSet["setcalib"] + "\n")
            read_until()
        else:
            print("command entered error: ", ipcmd)
    
#***************************************************
# call main function
#***************************************************

main()

