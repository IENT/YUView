from docx import Document
from parseVariableDescriptions import parseDocForVariableDescriptions
from parseTables import parseDocumentTables
import pickle

def main():
    #filename = "JVET-T2001-v2.docx"
    file = "/Users/cfeldman/Downloads/JVET-AN1019-v1/JVET-AN1019-v1_text.docx"
    print("Opening file " + file)
    document = Document(file)

    headingsToParse = ["Video usability information parameters", "SEI messages"]

    variableDescriptions = parseDocForVariableDescriptions(document, headingsToParse)

    print(f"Parsed {len(variableDescriptions)} variable descriptions.")
    
    parsedTables = parseDocumentTables(document, variableDescriptions)
    print ("Read {} classes".format(len(parsedTables)))

    # Dump everything to a file (debugging)
    pickle.dump( parsedTables, open( "tempPiclkle.p", "wb" ) )

if __name__ == "__main__":
    main()
