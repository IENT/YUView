

class VariableRestrictionRage:
    def __init__(self, min, max):
        self.min = min
        self.max = max

    def __str__(self):
        return f"Range({self.min}-{self.max})"


class VariableRestrictionGreaterThen:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"Greater({self.value})"


class VariableRestrictionEqualTo:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"Equal({self.value})"


class VariableDescription():
    def __init__(self, name):
        self.name = name
        self.description = ""
        self.shallBe = None

    def __str__(self):
        if (self.shallBe):
            return f"{self.name} -- {self.shallBe}"
        else:
            return f"{self.name}"

    def finishReading(self):
        self.lookForShallBe()

    def lookForShallBe(self):
        searchStrings = []
        searchStrings.append(self.name + " shall be ")
        searchStrings.append("the value of " + self.name + " shall be ")
        for searchString in searchStrings:
            posShallBe = self.description.lower().find(searchString)
            if (posShallBe != -1):
                posDot = self.description.find(".", posShallBe)
                if (posDot != -1):
                    self.parseRestriction(
                        self.description[posShallBe + len(searchString): posDot])
                    return

    def parseRestriction(self, restrictionText):
        if ("in the range of " in restrictionText):
            posMinStart = restrictionText.find(
                "in the range of ") + len("in the range of ")
            posMinEnd = restrictionText.find(" to ", posMinStart)
            posMaxStart = posMinEnd + len(" to ")
            posMaxEnd = restrictionText.find(", inclusive", posMaxStart)
            if (posMinStart == -1 or posMinEnd == -1 or posMaxStart == -1 or posMaxEnd == -1):
                print("Error parsing range")
                return
            self.shallBe = VariableRestrictionRage(
                restrictionText[posMinStart: posMinEnd], restrictionText[posMaxStart: posMaxEnd])
        elif ("greater than " in restrictionText):
            posValueStart = restrictionText.find(
                "greater then ") + len("greater then ")
            if (posValueStart == -1):
                print("Error parsing greater then")
                return
            self.shallBe = VariableRestrictionGreaterThen(
                restrictionText[posValueStart:])
        elif ("equal to " in restrictionText):
            posValueStart = restrictionText.find(
                "equal to ") + len("equal to ")
            if (posValueStart == -1):
                print("Error parsing equal to")
                return
            self.shallBe = VariableRestrictionEqualTo(
                restrictionText[posValueStart:])
        else:
            ignoreCases = ["the same for all pictures ", 
                           "the same in all ",
                           "the same for all PPSs "]
            for c in ignoreCases:
                if (c in restrictionText):
                    return
            print("TODO: Add this restriction: " + restrictionText)


def parseDocForVariableDescriptions(document):
    firstLastEntry = ["NAL unit header semantics", "Slice data semantics"]
    firstEntryFound = False
    variableDescriptions = []
    for idx, paragraph in enumerate(document.paragraphs):
        if (firstLastEntry[0] in paragraph.text):
            firstEntryFound = True
        if (firstLastEntry[1] in paragraph.text):
            break
        if (firstEntryFound):
            if (len(paragraph.runs) > 1):
                runText = paragraph.runs[0].text
                isBold = paragraph.runs[0].font.bold
                if (isBold):
                    if (len(variableDescriptions) > 0):
                        variableDescriptions[-1].finishReading()
                    variableDescriptions.append(VariableDescription(runText))
                    for i in range(len(paragraph.runs) - 1):
                        runText = paragraph.runs[i + 1].text
                        variableDescriptions[-1].description += runText
                    continue
            if (len(variableDescriptions) > 0):
                variableDescriptions[-1].description += paragraph.text
    
    return variableDescriptions
