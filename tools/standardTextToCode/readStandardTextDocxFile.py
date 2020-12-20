from docx import Document
from parseVariableDescriptions import parseDocForVariableDescriptions
from parseTables import parseDocumentTables
import pickle

def main():
    filename = "JVET-T2001-v2.docx"
    print("Opening file " + filename)
    document = Document(filename)

    variableDescriptions = parseDocForVariableDescriptions(document)

    print(f"Parsed {len(variableDescriptions)} variable descriptions: ")

    # From where to where to parse. The last entry will not be included.
    
    parsedTables = parseDocumentTables(document, variableDescriptions)
    print ("Read {} classes".format(len(parsedTables)))

    # Dump everything to a file (debugging)
    pickle.dump( parsedTables, open( "tempPiclkle.p", "wb" ) )

if __name__ == "__main__":
    main()
