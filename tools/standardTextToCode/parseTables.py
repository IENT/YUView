from codingType import Coding, CodingType, isCodingType
import re

def isVariableName(text : str):
    if ("[" in text and "]" in text):
        text = text.split("[")[0]  # Array indices are ok
    return re.fullmatch("[a-z][a-z0-9]*(_[a-z0-9]+)+", text)
def isFunctionCall(text : str):
    if (not "(" in text or not ")" in text):
        return False
    return isVariableName(text.split("(")[0].strip())
def removeComments(text : str):
    commentStart = text.find("/*")
    while (commentStart != -1):
        commentEnd = text.find("*/")
        if (commentEnd == -1):
            return text
        if (commentEnd <= commentStart):
            raise SyntaxError("Error removing comment. End before start. Line: " + text)
        text = text[0:commentStart] + text[commentEnd+2:]
        commentStart = text.find("/*")
    return text.strip()
def cleanCondition(text : str):
    text = text.strip()
    text = text.replace("=\xa0=", "==")
    text = text.replace("|\xa0|", "||")
    text = text.replace("[\xa0", "[")
    text = text.replace("\xa0]", "]")
    text = text.replace("\xa0", " ")
    text = text.replace("  ", " ")
    text = text.replace("\n", "")
    text = text.replace("\t", "")
    text = text.replace("\u2212", "-")
    return text
def cleanArgument(text : str):
    text = text.strip()
    text = text.replace("[\xa0", "[")
    text = text.replace("\xa0]", "]")
    text = text.replace("\xa0", " ")
    text = text.replace("\u2212", "-")
    return text
def cleanComment(text : str):
    text = text.strip()
    text = text.replace("=\xa0=", "==")
    text = text.replace("[\xa0", "[")
    text = text.replace("\xa0]", "]")
    text = text.replace("\xa0", " ")
    text = text.replace("\n", " ")
    text = text.replace("\t", "")
    text = text.replace("( ", "(")
    text = text.replace(" )", ")")
    text = text.replace("\u2212", "-")
    return text

def getEntryType(text : str):
    text = removeComments(text)
    if isVariableName(text):
        return "Variable"
    if isFunctionCall(text):
        return "FunctionCall"
    if text.startswith("for"):
        return "for"
    if text.startswith("if") or text.startswith("else if") or text.startswith("} else") or text.startswith("else"):
        return "if"
    if text.startswith("while"):
        return "while"
    if text.startswith("do"):
        return "do"

class ParsingItem:
    def __init__(self, parent):
        self.parent = parent
    
class Variable(ParsingItem):
    def __init__(self, parent):
        super().__init__(parent)
        self.name = None
        self.arrayIndex = None
        self.coding = None
        self.description = None
    def fromText(self, name : str, descriptor : str, variableDescriptions : dict):
        if ("[" in name and "]" in name):
            self.arrayIndex = []
            openBracket = name.find("[")
            self.name = name[0:openBracket]
            while (True):
                closeBracket = name.find("]")
                newIndex = cleanArgument(name[openBracket+1:closeBracket].strip())
                self.arrayIndex.append(newIndex)
                name = name[closeBracket+1:]
                openBracket = name.find("[")
                if (openBracket == -1):
                    break
        else:
            self.name = name
        if (self.name in variableDescriptions):
            self.description = variableDescriptions[self.name]
        self.coding = CodingType(descriptor)
    def __str__(self):
        s = ""
        for _ in range(self.parent.depth):
            s += "  "
        s += self.name
        if (self.arrayIndex):
            s += str(self.arrayIndex)
        return f"{s} --> {self.coding}"

class CommentEntry(ParsingItem):
    def __init__(self, parent):
        super().__init__(parent)
        self.text = None
    def fromText(self, text : str):
        self.text = cleanComment(text)
    def __str__(self):
        s = ""
        for _ in range(self.parent.depth):
            s += "  "
        return f"{s}//{self.text}"

class FunctionCall(ParsingItem):
    def __init__(self, parent):
        super().__init__(parent)
        self.functionName = None
        self.arguments = None
    def fromText(self, name : str):
        self.functionName = name.split("(")[0]
        self.arguments = []
        for argument in (name.split("(")[1].split(")")[0].split(",")):
            c = cleanArgument(argument)
            if (len(c) > 0):
                self.arguments.append(cleanArgument(argument))
        debugStop = 234
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
    def parseChildren(self, table, tableIndex, variableDescriptions):
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
                    v.fromText(t0, t1, variableDescriptions)
                    #print(f"{v}")
                    self.children.append(v)
                elif (entryType == "FunctionCall"):
                    f = FunctionCall(self)
                    f.fromText(t0)
                    #print(f"{f}")
                    self.children.append(f)
                elif (entryType == "for"):
                    f = ContainerFor(self)
                    f.fromText(t0)
                    #print(f"{f}")
                    self.children.append(f)
                    tableIndex = f.parseChildren(table, tableIndex, variableDescriptions)
                elif (entryType == "if"):
                    i = ContainerIf(self)
                    i.fromText(t0)
                    #print(f"{i}")
                    self.children.append(i)
                    tableIndex = i.parseChildren(table, tableIndex, variableDescriptions)
                elif (entryType == "while"):
                    w = ContainerWhile(self)
                    w.fromText(t0)
                    #print(f"{w}")
                    self.children.append(w)
                    tableIndex = w.parseChildren(table, tableIndex, variableDescriptions)
                elif (entryType == "do"):
                    d = ContainerDo(self)
                    d.fromText(t0)
                    #print(d.getDoText())
                    tableIndex = d.parseChildren(table, tableIndex, variableDescriptions)
                    tableIndex = d.parseClosingWhile(table, tableIndex)
                    #print(f"{d}")
                    self.children.append(d)
                elif (entryType != None):
                    debugStop = 2222
                else:
                    if (t0.strip() != "}"):
                        c = CommentEntry(self)
                        c.fromText(t0)
                        #print(f"{c}")
                        self.children.append(c)

                if (lastEntry):
                    return tableIndex
        except Exception as ex:
            print(f"Error parsing {self}: {ex}")
        return tableIndex

class ContainerTable(Container):
    def __init__(self):
        super().__init__(None)
    def parseContainer(self, table, variableDescriptions):
        self.parseHeader(table.cell(0, 0).text)
        t1 = table.cell(0, 1).text.strip()
        t2 = table.cell(0, 2).text.strip()
        if (t2 == t1):
            self.parseChildren(table, 3, variableDescriptions)
        else:
            self.parseChildren(table, 2, variableDescriptions)
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
        self.isElseIf = False
        self.isElse = False
    def fromText(self, text : str):
        if (not text.startswith("if") and not text.startswith("else if") and not text.startswith("} else") and not text.startswith("else")):
            raise SyntaxError("If container does not start with if or else if")
        if (text.startswith("else if")):
            self.isElseIf = True
        if (text.startswith("} else") or text.startswith("else")):
            self.isElse = True
            return
        start = text.find("(")
        end = text.rfind(")")
        if (start == -1 or end == -1):
            raise SyntaxError("If condition does not contain brackets")
        self.condition = cleanCondition(text[start+1:end])
    def __str__(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        if (self.isElse):
            return f"{spaces}else"
        if (self.isElseIf):
            return f"{spaces}else if({self.condition})"
        return f"{spaces}if({self.condition})"

class ContainerWhile(Container):
    def __init__(self, parent):
        super().__init__(parent)
        self.condition = None
    def fromText(self, text : str):
        if (not text.startswith("while")):
            raise SyntaxError("While container does not start with while")
        start = text.find("(")
        end = text.rfind(")")
        if (start == -1 or end == -1):
            raise SyntaxError("While loop does not contain brackets")
        self.condition = cleanCondition(text[start+1:end])
    def __str__(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        return f"{spaces}while({self.condition})"

class ContainerDo(Container):
    def __init__(self, parent):
        super().__init__(parent)
        self.condition = None
    def fromText(self, text : str):
        if (not text.startswith("do")):
            raise SyntaxError("Do container does not start with do")
    def parseClosingWhile(self, table, tableIndex : int):
        t0_full = table.cell(0, tableIndex).text
        text = t0_full.strip()
        if (not text.startswith("} while")):
            raise SyntaxError("do does not end with while")
        start = text.find("(")
        end = text.rfind(")")
        if (start == -1 or end == -1):
            raise SyntaxError("Do ... while loop does not contain brackets")
        self.condition = cleanCondition(text[start+1:end])
        t1 = table.cell(0, tableIndex+1).text.strip()
        try:
            t2 = table.cell(0, tableIndex+2).text.strip()
            if (t2 == t1):
                # Skip identical entries. This may be the aforementioned glitch.
                tableIndex += 1
        except IndexError:
            # No more data
            pass
        tableIndex += 2
        return tableIndex
    def getDoText(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        return f"{spaces}do"
    def __str__(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        return f"{spaces}while ({self.condition})"

class ContainerFor(Container):
    def __init__(self, parent):
        super().__init__(parent)
        self.variableName = None
        self.initialValue = None
        self.breakCondition = None
        self.increment = None
    def fromText(self, text : str):
        split = text.split(";")
        if (not split[0].startswith("for")):
            raise SyntaxError("For container does not start with for")
        
        firstPart = split[0][split[0].find("(") + 1:]
        self.variableName = firstPart.split("=")[0].strip()
        self.initialValue = firstPart.split("=")[1].strip()
        self.breakCondition = cleanCondition(split[1])
        self.increment = split[2][0:split[2].find(")")].strip()
    def __str__(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        return f"{spaces}for({self.variableName} = {self.initialValue}; {self.breakCondition}; {self.increment})"

def parseDocumentTables(document, variableDescriptions):
    parsedTables = []

    firstLastEntry = ["nal_unit_header", "slice_data"]
    skipEntries = ["sei_rbsp"]
    
    firstEntryFound = False
    for table in document.tables:
        entryName = table.cell(0, 0).text.split("(")[0]
        if (not firstEntryFound and entryName == firstLastEntry[0]):
            firstEntryFound = True
        if (firstEntryFound and entryName == firstLastEntry[1]):
            break
        if (entryName in skipEntries):
            continue
        if firstEntryFound:
            tableItem = ContainerTable()
            tableItem.parseContainer(table, variableDescriptions)
            parsedTables.append(tableItem)
    return parsedTables