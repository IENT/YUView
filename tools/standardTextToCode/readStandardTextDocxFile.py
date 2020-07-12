from docx import Document
from enum import Enum, unique, auto
from typing import List, NewType

@unique
class Coding(Enum):
    FIXED_CODE = auto()        # f(x) - fixed code to assert
    UNSIGNED_FIXED = auto()    # u(x) - unsigned int with a fixed number of bits
    UNSIGNED_VARIABLE = auto() # u(v) - unsigned int with a variable number of bits
    UNSIGNED_EXP = auto()      # ue(v) - unsigned int wit exp golomb coding
    SIGNED_EXP = auto()        # se(v) - signed int with exp golomb coding

class Variable:
    def __init__(self, name : str, descriptor : str):
        self.name = name
        if (descriptor.startswith("f(")):
            self.type = Coding.FIXED_CODE
            self.fixedLength = int(descriptor[2:-1])
        elif (descriptor.startswith("u(v)")):
            self.type = Coding.UNSIGNED_VARIABLE
        elif (descriptor.startswith("u(")):
            self.type = Coding.UNSIGNED_FIXED
            self.fixedLength = int(descriptor[2:-1])
        elif (descriptor.startswith("ue(v)")):
            self.type = Coding.UNSIGNED_EXP
        elif (descriptor.startswith("se(v)")):
            self.type = Coding.SIGNED_EXP
        else:
            raise SyntaxError("Unknown descriptor type " + descriptor)

class parsingClass:
    def __init__(self, table):
        self.parseHeader(table.cell(0, 0).text)
        self.variables = []
        self.parseCode(table)
    def __repr__(self):
        args = ",".join(self.arguments)
        return f"{self.name} ({args})"
    def parseHeader(self, header):
        header = header.replace(u'\xa0', u' ')
        bracketOpen = header.find("(")
        bracketClose = header.find(")")
        self.name = header[:bracketOpen]
        self.arguments = []
        for a in header[bracketOpen+1 : bracketClose].split(","):
            self.arguments.append(a.strip())
    def parseCode(self, table):
        # In case the table is not perfectly aligned on the right (as it is in the VVC standard text),
        # the table seems to be malformed here. 
        nrColumns = len(table.columns)
        if (nrColumns == 2):
            useColumns = (0, 1)
        elif (nrColumns == 3):
            useColumns = (1, 2)
        else:
            raise SyntaxError("Invalid number of columns in table")
        for row in range(1, len(table.rows)):
            text = table.cell(row, useColumns[0]).text.strip()
            descriptor = table.cell(row, useColumns[1]).text.strip()
            if (len(descriptor) > 0):
                self.variables.append(Variable(text, descriptor))

def main():
    filename = "JVET-R2001-vB.docx"
    print("Opening file " + filename)
    document = Document(filename)

    # From where to where to parse. The last entry will not be included.
    firstLastEntry = ["nal_unit_header", "slice_data"]

    firstEntryFound = False
    parsingClasses = []
    for table in document.tables:
        entryName = table.cell(0, 0).text.split("(")[0]
        if (not firstEntryFound and entryName == firstLastEntry[0]):
            firstEntryFound = True
        if (firstEntryFound and entryName == firstLastEntry[1]):
            break
        if firstEntryFound:
            c = parsingClass(table)
            parsingClasses.append(c)
    
    print ("Read {} classes".format(len(parsingClasses)))
    for i, c in enumerate(parsingClasses):
        print (str(i) + " - " + str(c))

if __name__ == "__main__":
    main()
