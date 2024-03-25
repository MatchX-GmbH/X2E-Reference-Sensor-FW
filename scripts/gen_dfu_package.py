# ===========================================================================
# Python3 script
# ===========================================================================
import os
import sys
import getopt
import zlib
import array
import json 
import zipfile

# ===========================================================================
# Global variables
# ===========================================================================
gVerbose = False
gInputFilePath = None

# ===========================================================================
# ===========================================================================
def showUsage():
    print("Usage: " + sys.argv[0] + " BIN_FILE")
    print("      -h, --help                 Show this help.")
    print("      -v, --verbose              Verbose output.")
    print(" ")


def getCommandLineArg():
    global gVerbose
    global gInputFilePath
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hc:v", [
            "help", "config="])
    except getopt.GetoptError as err:
        # print help information and exit:
        print(str(err))  # will print something like "option -a not recognized"
        showUsage()
        return False

    for o, a in opts:
        if o == "-v":
            gVerbose = True
        elif o in ("-h", "--help"):
            showUsage()
            sys.exit()
        else:
            print ("unhandled option")
            return False
        
    if len(args) > 0:
        gInputFilePath = args[0]

    return True

def calCrc(aFilePath):
    buffersize = 65536

    with open(aFilePath, 'rb') as afile:
        buffr = afile.read(buffersize)
        crcvalue = 0
        while len(buffr) > 0:
            crcvalue = zlib.crc32(buffr, crcvalue)
            buffr = afile.read(buffersize)
    return crcvalue & 0xFFFFFFFF

# ===========================================================================
# Main
# ===========================================================================

def main():
    if (getCommandLineArg() == False):
        sys.exit(1)
    if (gInputFilePath == None):
        showUsage()
        sys.exit(1)

    if not os.path.isfile(gInputFilePath):
        print ("File not found.")
        sys.exit(1)

    print("Binary file: " + gInputFilePath)
    crc_value = calCrc(gInputFilePath)
    print("CRC32: " + format(crc_value, '08x'))

    # Prepare tmp directoy
    tmp_dir = os.path.join("build","dfu")
    if os.path.exists(tmp_dir):
        os.system("rm -Rf " + tmp_dir)
    os.mkdir(tmp_dir)

    # construct file path
    input_basename = os.path.basename(gInputFilePath)
    input_part = os.path.splitext(input_basename)
    dat_filepath = os.path.join(tmp_dir, input_part[0] + ".dat")
    manifest_filepath = os.path.join(tmp_dir, "manifest.json")
    zip_filepath = os.path.join("build", input_part[0] + "_dfu.zip")

    # Dat file
    dat = array.array('B', [0]) * 16
    # Magic code
    dat[0] = 0x33
    dat[1] = 0x44
    dat[2] = 0x55
    dat[3] = 0x66
    # CRC32
    dat[4] = (crc_value >> 0) & 0xff
    dat[5] = (crc_value >> 8) & 0xff
    dat[6] = (crc_value >> 16) & 0xff
    dat[7] = (crc_value >> 24) & 0xff
    print("DFU init param: " + ", ".join(hex(b) for b in dat))

    print("Dat file: " + dat_filepath)
    with open(dat_filepath, "wb") as file:
        file.write(dat.tobytes())

    #
    os.system("cp " + gInputFilePath + " " + tmp_dir)

    # manifest.json
    manifest = {
        "manifest": {
            "application": {
                "bin_file": os.path.basename(gInputFilePath),
                "dat_file": os.path.basename(dat_filepath)
            }
        }
    }
    print("Manifest file: " + manifest_filepath)
    with open(manifest_filepath, "wt") as file:
        file.write(json.dumps(manifest, indent=4))

    # Zip
    print ("Zip file: " + zip_filepath)
    with zipfile.ZipFile(zip_filepath, mode="w") as archive:
        archive.write(gInputFilePath, os.path.basename(gInputFilePath))
        archive.write(dat_filepath, os.path.basename(dat_filepath))
        archive.write(manifest_filepath, os.path.basename(manifest_filepath))


# ===========================================================================
# ===========================================================================
if __name__ == "__main__":
    main()