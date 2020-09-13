
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


class VariableRestrictionLessThen:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"Less({self.value})"


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
        self.cleanDescriptionText()
        self.lookForShallBe()

    def cleanDescriptionText(self):
        self.description = self.description.replace(u'\xa0', u' ')

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
        conditionList = ["in the range of ", "greater than ",
                         "greater then ", "less than ", "less then ", "equal to "]
        for idx, condition in enumerate(conditionList):
            conditionPos = restrictionText.find(condition)
            conditionStart = conditionPos + len(condition)
            if (conditionPos != -1):
                conditionIndex = idx
                break
        if (conditionPos == -1):
            ignoreCases = ["the same for all pictures ",
                           "the same in all ",
                           "the same for all PPSs "]
            for c in ignoreCases:
                if (c in restrictionText):
                    return
            if (restrictionText == "0"):
                print("Warning: Restriction just says 'shall be 0'.")
                self.shallBe = VariableRestrictionEqualTo("0")
                return
            print("TODO: Add this restriction: " + restrictionText)
            return

        if (conditionIndex == 0):
            posMinEnd = restrictionText.find(" to ", conditionStart)
            posMaxStart = posMinEnd + len(" to ")
            posMaxEnd = restrictionText.find(", inclusive", posMaxStart)
            if (posMinEnd == -1 or posMaxStart == -1 or posMaxEnd == -1):
                print("Error parsing range")
                return
            self.shallBe = VariableRestrictionRage(
                restrictionText[conditionStart: posMinEnd], restrictionText[posMaxStart: posMaxEnd])
        elif (conditionIndex in [1, 2]):
            self.shallBe = VariableRestrictionGreaterThen(
                restrictionText[conditionStart:])
        elif (conditionIndex in [3, 4]):
            self.shallBe = VariableRestrictionLessThen(
                restrictionText[conditionStart:])
        elif (conditionIndex == 5):
            self.shallBe = VariableRestrictionEqualTo(
                restrictionText[conditionStart:])
        else:
            assert(False)

        if (conditionIndex in [2, 4]):
            print("Warning: Using then in a comparison.")


def parseDocForVariableDescriptions(document):
    firstLastEntry = ["NAL unit header semantics", "Slice data semantics"]
    firstEntryFound = False
    variableDescriptions = dict()
    currentDescription = None
    for paragraph in document.paragraphs:
        if (firstLastEntry[0] in paragraph.text):
            firstEntryFound = True
        if (firstLastEntry[1] in paragraph.text):
            break
        if (firstEntryFound):
            if (len(paragraph.runs) > 1):
                runText = paragraph.runs[0].text
                isBold = paragraph.runs[0].font.bold
                if (isBold):
                    if (currentDescription != None):
                        currentDescription.finishReading()
                        variableDescriptions[currentDescription.name] = currentDescription
                    # It can happen that the variable description is split over multiple
                    # runs. It will all be bold without spaces and can just be concatenated.
                    # Please don't ask me why.
                    runIndex = 1
                    for i in range(1, len(paragraph.runs)):
                        isBold = paragraph.runs[i].font.bold
                        text = paragraph.runs[i].text
                        containsSpaceAtEnd = (text[-1] == " ")
                        if (containsSpaceAtEnd):
                            text = text[:-1]
                        containesSpaces = (text.find(" ") != -1)
                        if (isBold and not containesSpaces):
                            runText += text
                        else:
                            break
                        if (containsSpaceAtEnd):
                            break
                        runIndex += 1
                    currentDescription = VariableDescription(runText)
                    # Get all the text after the variable name
                    for i in range(runIndex, len(paragraph.runs)):
                        runText = paragraph.runs[i].text
                        currentDescription.description += runText
                    continue
            if (currentDescription != None):
                currentDescription.description += paragraph.text

    if (currentDescription != None):
        currentDescription.finishReading()
        variableDescriptions[currentDescription.name] = currentDescription

    return variableDescriptions
