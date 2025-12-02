
import pickle
from parseTables import *
from pathlib import Path
from dataclasses import dataclass, field


@dataclass
class WritingSettings:
    outputPath: str = "cpp"
    namespace: str = "parser"
    baseClass: str = "NalRBSP"
    readerName: str = "SubByteReaderLogging"
    includes: list[str] = field(default_factory=list)


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
    def __init__ (self, settings: WritingSettings, name: str):
        self.f = open(f"{settings.outputPath}/{name}.h", "w")
        writeLicense(self.f)
        self.f.write("#pragma once\n\n")
        for include in settings.includes:
            self.f.write(f"#include \"{include}\"\n")
        self.f.write("\n")
        self.f.write(f"""namespace {settings.namespace}""")
        self.f.write("\n{\n\n")
        self.namespace = settings.namespace
        self.spaces = 0
    def __del__(self):
        self.f.write(f"""}} // namespace {self.namespace}""")
        self.f.write("\n")
    def write(self, s):
        for i in range(self.spaces):
            self.f.write(" ")
        self.f.write(s)

class CppFile:
    def __init__ (self, settings: WritingSettings, name: str):
        self.f = open(f"{settings.outputPath}/{name}.cpp", "w")
        writeLicense(self.f)
        self.f.write(f"""#include "{name}.h"\n""")
        self.f.write("""\n""")
        self.f.write(f"""namespace {settings.namespace}""")
        self.f.write("\n{\n\n")
        self.namespace = settings.namespace
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

def writeBeginningToHeader(table, file, readerName: str):
    if table.type == TableType.NAL_UNIT:
        file.write(f"class {table.name} : public NalRBSP\n")
    if table.type == TableType.SEI_MESSAGE:
        file.write(f"class {table.name} : public SEI\n")
    else:
        file.write(f"class {table.name}\n")
    file.write("{\n")
    file.write(f"public:\n")
    file.write(f"  {table.name}() = default;\n")
    file.write(f"  ~{table.name}() = default;\n")
    file.write(f"  void parse({readerName} &reader{argumentsToString(table.arguments, 'int')});\n\n")

def writeEndToHeader(file):
    file.spaces = 0
    file.write("};\n")
    file.write("\n")

def writeBeginnginToSource(table, file, readerName: str):
    file.write(f"void {table.name}::parse({readerName} &reader{argumentsToString(table.arguments, 'int')})\n")
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

def formatCondition(condition: str):
    if "byte_aligned" in condition:
        return "reader.byte_aligned()"
    return condition

def writeItemToFiles(item, files):
    header = files[0]
    cpp = files[1]
    if (type(item) == Variable):
        typeString = "unsigned"
        arguments = ""
        if item.coding.codingType == Coding.UNSIGNED_VARIABLE:
            nrBitsText = "variable"
            if item.description != None and item.description.variableParsingLength != None:
                nrBitsText = item.description.variableParsingLength
            typeString = "int"
            parseFunction = "readBits"
            arguments = f", {nrBitsText}"
        if (item.coding.codingType in [Coding.FIXED_CODE, Coding.UNSIGNED_FIXED]):
            if item.coding.length == 1:
                typeString = "bool"
                parseFunction = "readFlag"
            else:
                parseFunction = "readBits"
                arguments = f", {item.coding.length}"
        elif item.coding.codingType == Coding.UNSIGNED_EXP:
            parseFunction = "readUEV"
        elif item.coding.codingType == Coding.SIGNED_EXP:
            parseFunction = "readSEV"
            typeString = "int"
        elif item.coding.codingType == Coding.BYTE:
            parseFunction = "readBits"
            arguments = ", 8"
        elif item.coding.codingType == Coding.SIGNED_FIXED:
            parseFunction = "readBitsSigned"
            typeString = "int"
            arguments = f", {item.coding.length}"
        elif item.coding.codingType == Coding.STRING:
            parseFunction = "readString"
            typeString = "std::string"
        elif item.coding.codingType == Coding.UNKNOWN:
            parseFunction = "unknown"

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
        if item.isElse:
            cpp.write(f"else\n")
        elif item.isElseIf:
            cpp.write(f"else if ({formatCondition(item.condition)})\n")
        else:
            cpp.write(f"if ({formatCondition(item.condition)})\n")
        cpp.write("{\n")
        writeItemsInContainer(item, files)
        cpp.write("}\n")
    elif (type(item) == ContainerWhile):
        cpp.write(f"while ({formatCondition(item.condition)})\n")
        cpp.write("{\n")
        writeItemsInContainer(item, files)
        cpp.write("}\n")
    elif (type(item) == ContainerDo):
        cpp.write("do\n")
        cpp.write("{\n")
        writeItemsInContainer(item, files)
        cpp.write("} ")
        cpp.write(f"while({formatCondition(item.condition)})\n")
    elif (type(item) == ContainerFor):
        variableType = "unsigned"
        if ("--" in item.increment):
            variableType = "int"
        cpp.write(f"for ({variableType} {item.variableName} = {item.initialValue}; {item.breakCondition}; {item.increment})\n")
        cpp.write("{\n")
        writeItemsInContainer(item, files)
        cpp.write("}\n")

def writeTableToFiles(table, files, readerName):
    writeBeginningToHeader(table, files[0], readerName)
    writeBeginnginToSource(table, files[1], readerName)
    writeItemsInContainer(table, files)
    writeEndToSource(files[1])
    writeEndToHeader(files[0])

def writeTablesToCpp(parsedTables, settings: WritingSettings):
    Path(settings.outputPath).mkdir(parents=True, exist_ok=True)

    for table in parsedTables:
        assert(type(table) == ContainerTable)
        print(f"Writing {table.name}")
        files = (HeaderFile(settings, table.name), CppFile(settings, table.name))
        writeTableToFiles(table, files, settings.readerName)
        
def main():
    settings = WritingSettings()
    settings.includes = ["Units.h", "SubByteReaderDummy.h"]
    settings.readerName = "SubByteReaderDummy"

    parsedTables = pickle.load(open("tempPiclkle.p", "rb"))
    writeTablesToCpp(parsedTables, settings)

if __name__ == "__main__":
    main()
