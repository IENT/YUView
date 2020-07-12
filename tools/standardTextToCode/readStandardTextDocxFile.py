from docx import Document
from typing import List, NewType
from codingType import Coding, CodingType, isCodingType
import re

def getEntryType(text : str):
    if isVariableName(text):
        return "Variable"
    if text.startswith("for"):
        return "for"
    if text.startswith("if"):
        return "if"

class Variable:
    def __init__(self, name : str, descriptor : str):
        self.name = name
        self.coding = CodingType(descriptor)
    def __str__(self):
        return f"{self.name} --> {self.coding}"

def isVariableName(text : str):
    return re.fullmatch("[a-z][a-z0-9]*(_[a-z0-9]*)+", text)

class ContainerIf:
    def __init__(self, depth: int, description : str):
        self.condition = None
        self.depth = depth
        # TODO

class ContainerFor:
    def __init__(self, depth: int, description : str):
        self.variableName = None
        self.depth = depth
        self.breakCondition = None
        self.increment = None
        # TODO

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
        # the table seems to be malformed here. So we iterate through all items in the table
        # and just try to make the best of it.
        print(f"Parsing {self.name}")
        try:
            i = 2
            while (True):
                t0 = table.cell(0, i).text.strip()
                t1 = table.cell(0, i+1).text.strip()
                if (t0.startswith("nal_unit(")):
                    continue
                entryType = getEntryType(t0)
                if (entryType == "Variable"):
                    v = Variable(t0, t1)
                    print(f"V {v}")
                    self.variables.append(v)
                try:
                    t2 = table.cell(0, i+2).text.strip()
                except IndexError:
                    # No more data
                    return
                if (t2 == t1):
                    # Skip identical entries. This may be the aforementioned glitch.
                    i += 1
                i += 2
        except Exception as ex:
            print(f"Error parsing {self.name}: {ex}")
            return

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
