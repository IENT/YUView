
import pickle
from parseTables import *
from pathlib import Path

def writeLicense(writer):
    writer.write(
    """/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   In addition, as a special exception, the copyright holders give
 *   permission to link the code of portions of this program with the
 *   OpenSSL library under certain conditions as described in each
 *   individual source file, and distribute linked combinations including
 *   the two.
 *   
 *   You must obey the GNU General Public License in all respects for all
 *   of the code used other than OpenSSL. If you modify file(s) with this
 *   exception, you may extend this exception to your version of the
 *   file(s), but you are not obligated to do so. If you do not wish to do
 *   so, delete this exception statement from your version. If you delete
 *   this exception statement from all source files in the program, then
 *   also delete it here.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */\n\n""")

class HeaderFile:
    def __init__ (self, path, name, namespace):
        self.f = open(f"{path}/{name}.h", "w")
        writeLicense(self.f)
        self.f.write("#pragma once\n\n")
        self.f.write("#include \"NalUnitVVC.h\"\n")
        self.f.write("#include \"parser/common/ReaderHelperNew.h\"\n\n")
        self.f.write(f"""namespace {namespace}""")
        self.f.write("\n{\n\n")
        self.namespace = namespace
        self.spaces = 0
    def __del__(self):
        self.f.write(f"""}} // namespace {self.namespace}""")
        self.f.write("\n")
    def write(self, s):
        for i in range(self.spaces):
            self.f.write(" ")
        self.f.write(s)

class CppFile:
    def __init__ (self, path, name, namespace):
        self.f = open(f"{path}/{name}.cpp", "w")
        writeLicense(self.f)
        self.f.write(f"""#include "{name}.h"\n""")
        self.f.write("""\n""")
        self.f.write(f"""namespace {namespace}""")
        self.f.write("\n{\n\n")
        self.namespace = namespace
        self.spaces = 0
    def __del__(self):
        self.f.write(f"""}} // namespace {self.namespace}""")
        self.f.write("\n")
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

def writeBeginningToHeader(table, file):
    file.write(f"class {table.name} : public NalRBSP\n")
    file.write("{\n")
    file.write(f"public:\n")
    file.write(f"  {table.name}() = default;\n")
    file.write(f"  ~{table.name}() = default;\n")
    file.write(f"  void parse(ReaderHelperNew &reader{argumentsToString(table.arguments, 'int')});\n\n")

def writeEndToHeader(file):
    file.spaces = 0
    file.write("};\n")
    file.write("\n")

def writeBeginnginToSource(table, file):
    file.write(f"void {table.name}::parse(ReaderHelperNew &reader)\n")
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
        typeString = "unsigned"
        arguments = ""
        if (item.coding.codingType == Coding.UNSIGNED_VARIABLE and item.coding.length == 0):
            # Length for code should be known. Write the info and something that will not compile
            header.write(f"int {item.name} {{}};\n")
            cpp.write(f"""this->{item.name} = reader.readBits("{item.name}", unknown)\n""")
            return
        if (item.coding.codingType in [Coding.FIXED_CODE, Coding.UNSIGNED_VARIABLE, Coding.UNSIGNED_FIXED]):
            if (item.coding.length == 1):
                typeString = "bool"
                parseFunction = "readFlag"
            else:
                parseFunction = "readBits"
                arguments = f", {item.coding.length}"
        elif (item.coding.codingType == Coding.UNSIGNED_EXP):
            parseFunction = "readUEV"
        elif (item.coding.codingType == Coding.SIGNED_EXP):
            parseFunction = "readSEV"
            typeString = "int"

        name = item.name
        if (item.arrayIndex != None):
            for index in item.arrayIndex:
                name += f"[{index}]"
                typeString = f"std::vector<{typeString}>"
            cpp.write(f"""this->{item.name}.push_back(reader.{parseFunction}("{item.name}"{arguments}));\n""")
        else:
            cpp.write(f"""this->{name} = reader.{parseFunction}("{item.name}"{arguments});\n""")
        header.write(f"{typeString} {item.name} {{}};\n")
        
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
        variableType = "unsigned"
        if ("--" in item.increment):
            variableType = "int"
        cpp.write(f"for ({variableType} {item.variableName} = {item.initialValue}; {item.breakCondition}; {item.increment})\n")
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

    namespace = "parser::vvc"

    for table in parsedTables:
        assert(type(table) == ContainerTable)
        print(f"Writing {table.name}")
        files = (HeaderFile(path, table.name, namespace), CppFile(path, table.name, namespace))
        writeTableToFiles(table, files)
        
def main():
    parsedTables = pickle.load(open("tempPiclkle.p", "rb"))
    writeTablesToCpp(parsedTables, "cpp")

if __name__ == "__main__":
    main()
