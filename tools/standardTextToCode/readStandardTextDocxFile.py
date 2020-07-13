from docx import Document
from typing import List, NewType
from codingType import Coding, CodingType, isCodingType
import re
from abc import ABC, abstractmethod

def isVariableName(text : str):
    return re.fullmatch("[a-z][a-z0-9]*(_[a-z0-9]+)+", text)
def isFunctionCall(text : str):
    if (not "(" in text or not ")" in text):
        return False
    return isVariableName(text.split("(")[0].strip())

def getEntryType(text : str):
    if isVariableName(text):
        return "Variable"
    if isFunctionCall(text):
        return "FunctionCall"
    if text.startswith("for"):
        return "for"
    if text.startswith("if"):
        return "if"
    if text.startswith("while"):
        return "while"

class ParsingItem:
    def __init__(self, parent):
        self.parent = parent
    
class Variable(ParsingItem):
    def __init__(self, parent):
        super().__init__(parent)
        self.name = None
        self.coding = None
    def fromText(self, name : str, descriptor : str):
        self.name = name
        self.coding = CodingType(descriptor)
    def __str__(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        return f"{spaces}{self.name} --> {self.coding}"

class FunctionCall(ParsingItem):
    def __init__(self, parent):
        super().__init__(parent)
        self.functionName = None
        self.arguments = None
    def fromText(self, name : str):
        self.functionName = name.split("(")[0]
        self.arguments = name.split("(")[1].split(")")[0].split(",")
    def __str__(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        return f"{spaces}{self.functionName}({self.arguments})"

class Container(ParsingItem):
    def __init__(self, parent):
        super().__init__(parent)
        self.children = []
        self.depth = 0
        self.depth = 0
    def parseChildren(self, table, tableIndex):
        # Get the initial depth
        t0_full = table.cell(0, tableIndex).text
        self.depth = len(t0_full) - len(t0_full.lstrip("\t"))

        try:
            while (True):
                t0_full = table.cell(0, tableIndex).text
                newDepth = len(t0_full) - len(t0_full.lstrip("\t"))
                if (newDepth < self.depth):
                    # End of container
                    return tableIndex
                if (newDepth > self.depth):
                    raise SyntaxError(f"The depth of the line is higher then the container depth. Line: {t0_full}")
                t0 = t0_full.strip()
                t1 = table.cell(0, tableIndex+1).text.strip()
                entryType = getEntryType(t0)

                lastEntry = False
                try:
                    t2 = table.cell(0, tableIndex+2).text.strip()
                    if (t2 == t1):
                        # Skip identical entries. This may be the aforementioned glitch.
                        tableIndex += 1
                except IndexError:
                    # No more data
                    lastEntry = True
                tableIndex += 2

                if (entryType == "Variable"):
                    v = Variable(self)
                    v.fromText(t0, t1)
                    print(f"{v}")
                    self.children.append(v)
                elif (entryType == "FunctionCall"):
                    f = FunctionCall(self)
                    f.fromText(t0)
                    print(f"{f}")
                    self.children.append(f)
                elif (entryType == "for"):
                    f = ContainerFor(self)
                    f.fromText(t0)
                    print(f"{f}")
                    self.children.append(v)
                    tableIndex = f.parseChildren(table, tableIndex)
                elif (entryType == "if"):
                    i = ContainerIf(self)
                    i.fromText(t0)
                    print(f"{i}")
                    self.children.append(i)
                    tableIndex = i.parseChildren(table, tableIndex)
                elif (entryType == "while"):
                    w = ContainerWhile(self)
                    w.fromText(t0)
                    print(f"{w}")
                    self.children.append(w)
                    tableIndex = w.parseChildren(table, tableIndex)
                elif (entryType != None):
                    debugStop = 2222

                if (lastEntry):
                    return tableIndex
        except Exception as ex:
            print(f"Error parsing {self}: {ex}")
        return tableIndex

class ContainerTable(Container):
    def __init__(self):
        super().__init__(None)
    def parseContainer(self, table):
        self.parseHeader(table.cell(0, 0).text)
        t1 = table.cell(0, 1).text.strip()
        t2 = table.cell(0, 2).text.strip()
        if (t2 == t1):
            self.parseChildren(table, 3)
        else:
            self.parseChildren(table, 2)
    def parseHeader(self, header):
        header = header.replace(u'\xa0', u' ')
        bracketOpen = header.find("(")
        bracketClose = header.find(")")
        self.name = header[:bracketOpen]
        self.arguments = []
        for a in header[bracketOpen+1 : bracketClose].split(","):
            self.arguments.append(a.strip())
        print(f"Table: {self.name}")
    
class ContainerIf(Container):
    def __init__(self, parent):
        super().__init__(parent)
        self.condition = None
    def fromText(self, text : str):
        split = re.split(";|\)|\(", text)
        if (split[0].strip() != "if"):
            raise SyntaxError("If container does not start with if")
        self.condition = split[1].strip()
    def __str__(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        return f"{spaces}if({self.condition})"

class ContainerWhile(Container):
    def __init__(self, parent):
        super().__init__(parent)
        self.condition = None
    def fromText(self, text : str):
        split = re.split(";|\)|\(", text)
        if (split[0].strip() != "while"):
            raise SyntaxError("While container does not start with while")
        self.condition = split[1].strip()
    def __str__(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        return f"{spaces}while({self.condition})"

class ContainerFor(Container):
    def __init__(self, parent):
        super().__init__(parent)
        self.variableName = None
        self.initialValue = None
        self.breakCondition = None
        self.increment = None
    def fromText(self, text : str):
        split = re.split(";|\)|\(", text)
        if (split[0].strip() != "for"):
            raise SyntaxError("For container does not start with for")
        self.variableName = split[1].strip().split("=")[0].strip()
        self.initialValue = int(split[1].strip().split("=")[1].strip())
        self.breakCondition = split[2].strip().strip()
        self.increment = split[3].strip().strip()
    def __str__(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        return f"{spaces}for({self.variableName} = {self.initialValue}; {self.breakCondition}; {self.increment})"
    
def main():
    filename = "JVET-R2001-vB.docx"
    print("Opening file " + filename)
    document = Document(filename)

    # From where to where to parse. The last entry will not be included.
    firstLastEntry = ["nal_unit_header", "slice_data"]

    firstEntryFound = False
    parsedTables = []
    for table in document.tables:
        entryName = table.cell(0, 0).text.split("(")[0]
        if (not firstEntryFound and entryName == firstLastEntry[0]):
            firstEntryFound = True
        if (firstEntryFound and entryName == firstLastEntry[1]):
            break
        if firstEntryFound:
            tableItem = ContainerTable()
            tableItem.parseContainer(table)
            parsedTables.append(tableItem)
        if (len(parsedTables) > 3):
            # TODO: REMOVE
            break
    
    print ("Read {} classes".format(len(parsedTables)))
    for i, c in enumerate(parsedTables):
        print (str(i) + " - " + str(c))

if __name__ == "__main__":
    main()
