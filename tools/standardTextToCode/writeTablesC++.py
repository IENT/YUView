
import pickle
from parseTables import *
from pathlib import Path

class VVCDefinitions:
    vpsName = "video_parameter_set_rbsp"
    spsName = "seq_parameter_set_rbsp"
    ppsName = "pic_parameter_set_rbsp"

def writeBeginningToHeader(table, file):
    file.write(f"class {table.name}\n")
    file.write("{\n")
    file.write(f"public:\n")
    file.write(f"  {table.name}();\n")
    file.write(f"  ~{table.name}() = default;\n")
    file.write(f"  void parse(Reader &reader);\n")
    file.write(f"private:\n")

def writeEndToHeader(file):
    file.write("};\n")
    file.write("\n")

def writeBeginnginToSource(table, file):
    file.write(f"void {table.name}::parse(Reader &reader)\n")
    file.write("{\n")

def writeEndToSource(file):
    file.write("}\n")
    file.write("\n")

def writeItemToFiles(item, files):
    header = files[0]
    cpp = files[1]
    if (type(item) == Variable):
        typeString = "int"
        arguments = ""
        if (item.coding.codingType == Coding.UNSIGNED_VARIABLE and item.coding.length == 0):
            # Length for code should be known. Write the info and something that will not compile
            header.write(f"  unknown {item.name};\n")
            cpp.write(f"  reader.(unknown parameters)\n")
            return
        if (item.coding.codingType in [Coding.FIXED_CODE, Coding.UNSIGNED_VARIABLE, Coding.UNSIGNED_FIXED]):
            if (item.coding.length == 1):
                typeString = "bool"
                parseFunction = "readFlag"
            else:
                typeString = "int"
                parseFunction = "readBits"
                arguments = f", {item.coding.length}"
        elif (item.coding.codingType == Coding.UNSIGNED_EXP):
            parseFunction = "readUEV"
        elif (item.coding.codingType == Coding.SIGNED_EXP):
            parseFunction = "readSEV"

        if (item.arrayIndex != None):
            typeString = f"std::vector<{typeString}>"
        header.write(f"  {typeString} {item.name};\n")
        cpp.write(f"""  reader.{parseFunction}("{item.name}"{arguments});\n""")

def writeTableToFiles(table, files):
    writeBeginningToHeader(table, files[0])
    writeBeginnginToSource(table, files[1])
    for item in table.children:
        writeItemToFiles(item, files)
    writeEndToSource(files[1])
    writeEndToHeader(files[0])

def writeTablesToCpp(parsedTables, path):
    Path(path).mkdir(parents=True, exist_ok=True)

    othersFiles = (open(path + "/other.h", "w"), open(path + "/other.cpp", "w"))
    for table in parsedTables:
        assert(type(table) == ContainerTable)
        print(table.name)
        if (table.name == VVCDefinitions.vpsName):
            files = (open(path + "/vps.h", "w"), open(path + "/vps.cpp", "w"))
            writeTableToFiles(table, files)
        elif (table.name == VVCDefinitions.spsName):
            files = (open(path + "/sps.h", "w"), open(path + "/sps.cpp", "w"))
            writeTableToFiles(table, files)
        elif (table.name == VVCDefinitions.ppsName):
            files = (open(path + "/pps.h", "w"), open(path + "/pps.cpp", "w"))
            writeTableToFiles(table, files)
        else:
            writeTableToFiles(table, othersFiles)

def main():
    parsedTables = pickle.load(open("tempPiclkle.p", "rb"))
    writeTablesToCpp(parsedTables, "cpp")

if __name__ == "__main__":
    main()
