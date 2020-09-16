
import pickle
from parseTables import *
from pathlib import Path

class OutputFile:
    def __init__(self, name, classes, dependencies):
        self.name = name
        self.classes = classes
        self.dependencies = dependencies
        self.tables = []

class HeaderFile:
    def __init__ (self, path, name, dependencies):
        self.f = open(f"{path}/{name}.h", "w")
        self.f.write("""#include "common.h"\n""")
        self.f.write("""#include <vector>\n\n""")
        for dep in dependencies:
            self.f.write(f"""#include "{dep}.h"\n""")
        self.spaces = 0
    def write(self, s):
        for i in range(self.spaces):
            self.f.write(" ")
        self.f.write(s)

class CppFile:
    def __init__ (self, path, name):
        self.f = open(f"{path}/{name}.cpp", "w")
        self.f.write(f"""#include "{name}.h"\n""")
        self.f.write("#include <vector>\n\n")
        self.spaces = 0
    def write(self, s):
        for i in range(self.spaces):
            self.f.write(" ")
        self.f.write(s)

def argumentsToString(arguments, variableType = ""):
    s = ""
    if (variableType != ""):
        variableType += " "
    newList = [x for x in arguments if x.strip() != ""]
    for i, arg in enumerate(newList):
        s += f", {variableType}{arg}"
    return s

def writeOutCommonClasses(file):
    file.write("#include <string>\n")
    file.write("\n")
    file.write("class Reader\n")
    file.write("{\n")
    file.write("public:\n")
    file.write("  bool     readFlag(std::string name) { return false; }\n")
    file.write("  unsigned readBits(std::string name, unsigned nrBits) { return 1; }\n")
    file.write("  unsigned readUEV(std::string name) { return 1; }\n")
    file.write("  int      readSEV(std::string name) { return 1; }\n")
    file.write("};\n")

def writeBeginningToHeader(table, file):
    file.write(f"class {table.name}\n")
    file.write("{\n")
    file.write(f"public:\n")
    file.write(f"  {table.name}();\n")
    file.write(f"  ~{table.name}() = default;\n")
    file.write(f"  void parse(Reader &reader{argumentsToString(table.arguments, 'int')});\n")
    file.write(f"private:\n")

def writeEndToHeader(file):
    file.spaces = 0
    file.write("};\n")
    file.write("\n")

def writeBeginnginToSource(table, file):
    file.write(f"void {table.name}::parse(Reader &reader)\n")
    file.write("{\n")

def writeEndToSource(file):
    file.write("}\n")
    file.write("\n")

def writeItemsInContainer(container, files):
    if (files[0].spaces == 0):
        files[0].spaces = 2
    files[1].spaces += 2
    for item in container.children:
        writeItemToFiles(item, files)
    files[1].spaces -= 2

def writeItemToFiles(item, files):
    header = files[0]
    cpp = files[1]
    if (type(item) == Variable):
        typeString = "int"
        arguments = ""
        if (item.coding.codingType == Coding.UNSIGNED_VARIABLE and item.coding.length == 0):
            # Length for code should be known. Write the info and something that will not compile
            header.write(f"int {item.name};\n")
            cpp.write(f"""this->{item.name} = reader.readBits("{item.name}", unknown)\n""")
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

        name = item.name
        if (item.arrayIndex != None):
            for index in item.arrayIndex:
                name += f"[{index}]"
                typeString = f"std::vector<{typeString}>"
        header.write(f"{typeString} {item.name};\n")
        cpp.write(f"""this->{name} = reader.{parseFunction}("{item.name}"{arguments});\n""")
    elif (type(item) == CommentEntry):
        header.write(f"//{item.text}\n")
    elif (type(item) == FunctionCall):
        header.write(f"{item.functionName} {item.functionName}_instance;\n")
        cpp.write(f"this->{item.functionName}_instance.parse(reader{argumentsToString(item.arguments)});\n")
    elif (type(item) == ContainerIf):
        cpp.write(f"if ({item.condition})\n")
        cpp.write("{\n")
        writeItemsInContainer(item, files)
        cpp.write("}\n")
    elif (type(item) == ContainerWhile):
        cpp.write(f"while ({item.condition})\n")
        cpp.write("{\n")
        writeItemsInContainer(item, files)
        cpp.write("}\n")
    elif (type(item) == ContainerDo):
        cpp.write("do\n")
        cpp.write("{\n")
        writeItemsInContainer(item, files)
        cpp.write("} ")
        cpp.write(f"while({item.condition})\n")
    elif (type(item) == ContainerFor):
        cpp.write(f"for (int {item.variableName} = {item.initialValue}; {item.breakCondition}; i += {item.increment})\n")
        cpp.write("{\n")
        writeItemsInContainer(item, files)
        cpp.write("}\n")

def writeTableToFiles(table, files):
    writeBeginningToHeader(table, files[0])
    writeBeginnginToSource(table, files[1])
    writeItemsInContainer(table, files)
    writeEndToSource(files[1])
    writeEndToHeader(files[0])

def writeTablesToCpp(parsedTables, path):
    Path(path).mkdir(parents=True, exist_ok=True)

    writeOutCommonClasses(open(path + "/common.h", "w"))

    vvcOutputFiles = [
        OutputFile("vps", ["video_parameter_set_rbsp"], ["ptl", "dpb_parameters", "general_hrd_parameters", "ols_hrd_parameters"]),
        OutputFile("sps", ["seq_parameter_set_rbsp"], ["ptl", "dpb_parameters", "ref_pic_list_struct", "general_hrd_parameters", "ols_hrd_parameters", "vui"]),
        OutputFile("pps", ["pic_parameter_set_rbsp"], []),
        OutputFile("ptl", ["profile_tier_level"], []),
        OutputFile("dpb_parameters", ["dpb_parameters"], []),
        OutputFile("general_hrd_parameters", ["general_hrd_parameters"], []),
        OutputFile("ols_hrd_parameters", ["ols_hrd_parameters"], []),
        OutputFile("vui", ["vui_parameters"], []),
        OutputFile("other", [], [])
    ]

    for table in parsedTables:
        assert(type(table) == ContainerTable)
        print(table.name)
        # Sort the tables into the output files
        for outFile in vvcOutputFiles:
            for includeClass in outFile.classes:
                if (includeClass == table.name):
                    outFile.tables.append(table)
                    break
            if (outFile.name == "other"):
                outFile.tables.append(table)

    for outFile in vvcOutputFiles:
        files = (HeaderFile(path, outFile.name, outFile.dependencies), CppFile(path, outFile.name))
        for table in outFile.tables:
            writeTableToFiles(table, files)

def main():
    parsedTables = pickle.load(open("tempPiclkle.p", "rb"))
    writeTablesToCpp(parsedTables, "cpp")

if __name__ == "__main__":
    main()
