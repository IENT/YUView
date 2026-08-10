
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
    def __init__(self, names):
        self.names = names
        self.description = ""
        self.shallBe = None
        self.variableParsingLength = None

    def __str__(self):
        if (self.shallBe):
            return f"{self.names} -- {self.shallBe}"
        else:
            return f"{self.names}"

    def finishReading(self):
        if self.names == None or len(self.names) == 0:
            print(
                f"Warning: No names for description {self.description}. Ignoring.")
            return
        self.cleanDescriptionText()
        self.lookForShallBe()
        self.lookForVariableParsingLength()

    def cleanDescriptionText(self):
        self.description = self.description.replace(u'\xa0', u' ')

    def lookForShallBe(self):
        searchStrings = []
        searchStrings.append(self.names[0] + " shall be ")
        searchStrings.append("the value of " + self.names[0] + " shall be ")
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
                print(f"Error parsing range: {restrictionText}")
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
            assert (False)

        if (conditionIndex in [2, 4]):
            print("Warning: Using then in a comparison.")

    def lookForVariableParsingLength(self):
        descriptionLower = self.description.lower()
        lengthFormulations = [("the number of bits used for ", "syntax element is"),
                              ("the length of the ", "syntax element is"), ("the length of ", " is ")]

        for partA, partB in lengthFormulations:
            posSentenceStart = descriptionLower.find(partA)
            if posSentenceStart == -1:
                continue
            posNextPeriod = descriptionLower.find(".", posSentenceStart)
            posTextBeforeValue = descriptionLower.find(partB, posSentenceStart)
            if posNextPeriod == -1 or posTextBeforeValue == -1 or posNextPeriod < posTextBeforeValue:
                continue

            self.variableParsingLength = self.description[posTextBeforeValue +
                                    len(partB):posNextPeriod].strip()
            if self.variableParsingLength.endswith(" bits"):
                self.variableParsingLength = self.variableParsingLength[:-len(" bits")]

            return


def extractVariableNamesFromRuns(runs):
    # It can happen that the variable description is split over multiple
    # runs. It will all be bold without spaces and can just be concatenated.
    # Please don't ask me why.
    names = []
    currentName = ""
    for run in runs:
        text = run.text.strip()
        if run.font.bold and not " " in text:
            currentName += text
        elif not run.font.bold and (text == "and" or text == ","):
            if currentName != "":
                names.append(currentName)
            currentName = ""
        else:
            names.append(currentName)
            break
    return names


def parseDocForVariableDescriptions(document, headingsToParse):
    parsingEnabled = False
    variableDescriptions = []
    currentDescription = None
    for paragraph in document.paragraphs:
        if paragraph.style.name == "Heading 1":
            parsingEnabled = (paragraph.text in headingsToParse)
        if not parsingEnabled:
            continue
        if paragraph.style.name.startswith("Heading"):
            continue

        if (len(paragraph.runs) > 1):
            isBold = paragraph.runs[0].font.bold
            if (isBold):
                if (currentDescription != None):
                    currentDescription.finishReading()
                    variableDescriptions.append(currentDescription)

                variableNames = extractVariableNamesFromRuns(paragraph.runs)

                currentDescription = VariableDescription(variableNames)

                for run in paragraph.runs:
                    currentDescription.description += run.text
                continue
        if (currentDescription != None):
            currentDescription.description += paragraph.text

    if (currentDescription != None):
        currentDescription.finishReading()
        variableDescriptions.append(currentDescription)

    return variableDescriptions
