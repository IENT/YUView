from codingType import Coding, CodingType, isCodingType
import re
from enum import Enum, unique, auto


def isVariableName(text: str):
    if ("[" in text and "]" in text):
        text = text.split("[")[0]  # Array indices are ok
    return re.fullmatch("[a-z][a-z0-9]*(_[a-z0-9]+)+", text)


def isFunctionCall(text: str):
    if (not "(" in text or not ")" in text):
        return False
    return isVariableName(text.split("(")[0].strip())


def removeComments(text: str):
    commentStart = text.find("/*")
    while (commentStart != -1):
        commentEnd = text.find("*/")
        if (commentEnd == -1):
            return text
        if (commentEnd <= commentStart):
            raise SyntaxError(
                "Error removing comment. End before start. Line: " + text)
        text = text[0:commentStart] + text[commentEnd+2:]
        commentStart = text.find("/*")
    return text.strip()


def cleanCondition(text: str):
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
    text = text.replace('−', '-')
    if (text.find("\xa0") != -1):
        raise SyntaxError(
            "There still is a char to replace in the condition. This must be cleaned up first.")
    return text


def cleanArgument(text: str):
    text = text.strip()
    text = text.replace("[\xa0", "[")
    text = text.replace("\xa0]", "]")
    text = text.replace("\xa0", " ")
    text = text.replace("\u2212", "-")
    text = text.replace('−', '-')
    if (text.find("\xa0") != -1):
        raise SyntaxError(
            "There still is a char to replace in the argument. This must be cleaned up first.")
    return text


def cleanComment(text: str):
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
    text = text.replace('−', '-')
    if (text.find("\xa0") != -1):
        raise SyntaxError(
            "There still is a char to replace in the comment. This must be cleaned up first.")
    return text


def cleanConditionPart(text: str):
    text = text.strip()
    text = text.replace("\xa0−\xa0", " - ")
    text = text.replace('−', '-')
    if (text.find("\xa0") != -1):
        raise SyntaxError(
            "There still is a char to replace in the condition. This must be cleaned up first.")
    return text


def cleanIncrement(text: str):
    text = text.strip()
    text = text.replace("-\xa0-", "--")
    text = text.replace("−\xa0−", "--")
    text = text.replace('−', '-')
    if (text.find("\xa0") != -1):
        raise SyntaxError(
            "There still is a char to replace in the increment. This must be cleaned up first.")
    return text


def getEntryType(text: str):
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


def tryFindVariableDescription(name, variableDescriptions):
    for description in variableDescriptions:
        if name in description.names:
            return description
    return None


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

    def fromText(self, name: str, descriptor: str, variableDescriptions: dict):
        if ("[" in name and "]" in name):
            self.arrayIndex = []
            openBracket = name.find("[")
            self.name = name[0:openBracket]
            while (True):
                closeBracket = name.find("]")
                newIndex = cleanArgument(
                    name[openBracket+1:closeBracket].strip())
                self.arrayIndex.append(newIndex)
                name = name[closeBracket+1:]
                openBracket = name.find("[")
                if (openBracket == -1):
                    break
        else:
            self.name = name
        self.description = tryFindVariableDescription(
            self.name, variableDescriptions)
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

    def fromText(self, text: str):
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

    def fromText(self, name: str):
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
        self.depth = None

    def parseChildren(self, table, rowIndex, currentDepth, variableDescriptions):
        try:
            while (True):
                rawSymbol = table.cell(rowIndex, 0).text

                startsWithComment = rawSymbol.lstrip("\t").startswith("/*")

                newDepth = len(rawSymbol) - len(rawSymbol.lstrip("\t"))
                if currentDepth == None:
                    currentDepth = newDepth
                if newDepth < currentDepth and not startsWithComment:
                    return rowIndex
                elif newDepth > currentDepth:
                    raise SyntaxError(
                        f"The depth of the line is higher then the container depth. This should only happen when entering a container (e.g. if, else, while). Symbol: {symbol}")

                symbol = table.cell(rowIndex, 0).text.strip()
                coding = table.cell(rowIndex, 1).text.strip()

                entryType = getEntryType(symbol)

                # print(f"Parsing entry: {symbol}")
                if (entryType == "Variable"):
                    v = Variable(self)
                    v.fromText(symbol, coding, variableDescriptions)
                    # print(f"{v}")
                    self.children.append(v)
                    rowIndex += 1
                elif (entryType == "FunctionCall"):
                    f = FunctionCall(self)
                    f.fromText(symbol)
                    # print(f"{f}")
                    self.children.append(f)
                    rowIndex += 1
                elif (entryType == "for"):
                    f = ContainerFor(self)
                    f.fromText(symbol)
                    # print(f"{f}")
                    self.children.append(f)
                    rowIndex = f.parseChildren(
                        table, rowIndex + 1, currentDepth + 1, variableDescriptions)
                elif (entryType == "if"):
                    i = ContainerIf(self)
                    i.fromText(symbol)
                    # print(f"{i}")
                    self.children.append(i)
                    rowIndex = i.parseChildren(
                        table, rowIndex + 1, currentDepth + 1, variableDescriptions)
                elif (entryType == "while"):
                    w = ContainerWhile(self)
                    w.fromText(symbol)
                    # print(f"{w}")
                    self.children.append(w)
                    rowIndex = w.parseChildren(
                        table, rowIndex + 1, currentDepth + 1, variableDescriptions)
                elif (entryType == "do"):
                    d = ContainerDo(self)
                    d.fromText(symbol)
                    # print(d.getDoText())
                    rowIndex = d.parseChildren(
                        table, rowIndex + 1, currentDepth + 1, variableDescriptions)
                    rowIndex = d.parseClosingWhile(table, rowIndex)
                    # print(f"{d}")
                    self.children.append(d)
                elif (entryType == "comment"):
                    c = CommentEntry(self)
                    c.fromText(symbol)
                    # print(f"{c}")
                    self.children.append(c)
                    rowIndex += 1
                elif (entryType != None):
                    raise SyntaxError(
                        f"Entry type is unknown. Line: {symbol}")
                elif symbol == "}":
                    rowIndex += 1
                else:
                    c = CommentEntry(self)
                    c.fromText(symbol)
                    # print(f"{c}")
                    self.children.append(c)
                    rowIndex += 1

                if (rowIndex + 1 >= len(table.rows)):
                    return rowIndex
        except Exception as ex:
            print(f"Error parsing {self}: {ex}")
            if hasattr(self, "name"):
                print(f"In table {self.name}")
        return rowIndex


@unique
class TableType(Enum):
    NAL_UNIT = auto()     # A full NAL unit
    # An SEI message. This knows its payload size when reading.
    SEI_MESSAGE = auto()
    # An element (a function) that is part of an SEI or a NAL unit.
    ELEMENT = auto()


class ContainerTable(Container):
    def __init__(self):
        super().__init__(None)
        self.name = ""
        self.type = None
        self.arguments = None

    def parseContainer(self, table, variableDescriptions):
        self.parseHeader(table.cell(0, 0).text)
        if len(self.arguments) == 0:
            self.type = TableType.NAL_UNIT
        elif len(self.arguments) == 1 and self.arguments[0] == "payloadSize":
            self.type = TableType.SEI_MESSAGE
        else:
            self.type = TableType.ELEMENT
        t1 = table.cell(0, 1).text.strip()
        if not "descriptor" in table.cell(0, 1).text.strip().lower():
            print(
                f"Warning: Table header column 2 does not contain 'descriptor' heading in table {self.name}")
        self.parseChildren(table, 1, None, variableDescriptions)

    def parseHeader(self, header):
        header = header.replace(u'\xa0', u' ')
        bracketOpen = header.find("(")
        bracketClose = header.find(")")
        if bracketOpen == -1 or bracketClose == -1:
            raise SyntaxError(
                f"Table header does not contain brackets: {header}")
        self.name = header[:bracketOpen]
        self.arguments = []
        for a in header[bracketOpen+1: bracketClose].split(","):
            self.arguments.append(a.strip())


class ContainerIf(Container):
    def __init__(self, parent):
        super().__init__(parent)
        self.condition = None
        self.isElseIf = False
        self.isElse = False

    def fromText(self, text: str):
        if (not text.startswith("if") and not text.startswith("else if") and not text.startswith("} else") and not text.startswith("else")):
            raise SyntaxError("If container does not start with if or else if")
        elif (text.startswith("else if") or text.startswith("} else if")):
            self.isElseIf = True
        elif (text.startswith("} else") or text.startswith("else")):
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

    def fromText(self, text: str):
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

    def fromText(self, text: str):
        if (not text.startswith("do")):
            raise SyntaxError("Do container does not start with do")

    def parseClosingWhile(self, table, rowIndex: int):
        symbol = table.cell(rowIndex, 0).text.strip()
        if (not symbol.startswith("} while")):
            raise SyntaxError("do does not end with while")
        start = symbol.find("(")
        end = symbol.rfind(")")
        if (start == -1 or end == -1):
            raise SyntaxError("Do ... while loop does not contain brackets")
        self.condition = cleanCondition(symbol[start+1:end])
        return rowIndex + 1

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

    def fromText(self, text: str):
        split = text.split(";")
        if (not split[0].startswith("for")):
            raise SyntaxError(f"For container does not start with for - {text}")
        if (len(split) != 3):
            raise SyntaxError(f"For container does not have exactly three parts - {text}")

        firstPart = split[0][split[0].find("(") + 1:]
        self.variableName = cleanConditionPart(firstPart.split("=")[0])
        self.initialValue = cleanConditionPart(firstPart.split("=")[1])
        self.breakCondition = cleanCondition(split[1])
        self.increment = cleanIncrement(split[2][0:split[2].find(")")])

    def __str__(self):
        spaces = ""
        for _ in range(self.parent.depth):
            spaces += "  "
        return f"{spaces}for({self.variableName} = {self.initialValue}; {self.breakCondition}; {self.increment})"


def parseDocumentTables(document, variableDescriptions):
    parsedTables = []

    startEntries = ["vui_parameters", "filler_payload"]
    endEntries = ["vui_parameters", "reserved_message"]
    skipEntries = ["sei_rbsp"]

    parsingEnabled = False
    for table in document.tables:
        if len(table.rows) == 0 or len(table.columns) != 2:
            continue
        firstCell = table.cell(0, 0)
        if firstCell.text == "Value" and firstCell.paragraphs[0].style.name == "Table_head":
            continue
        entryName = firstCell.text.split("(")[0]
        if not parsingEnabled and entryName in startEntries:
            parsingEnabled = True
        if entryName in skipEntries or entryName.strip() == "" or entryName.startswith("Table"):
            continue
        if parsingEnabled:
            try:
                tableItem = ContainerTable()
                tableItem.parseContainer(table, variableDescriptions)
                if tableItem.name == "":
                    print("Warning: Table with empty name encountered. Ignoring Table.")
                else:
                    print(f"Parsed Table: {tableItem.name}")
                    parsedTables.append(tableItem)
            except Exception as ex:
                print(f"Error parsing table {firstCell.text} - {ex}")
        if (parsingEnabled and entryName in endEntries):
            parsingEnabled = False
    return parsedTables
