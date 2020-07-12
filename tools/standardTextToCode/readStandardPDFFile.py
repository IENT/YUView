from PyPDF3 import PdfFileReader

def main():
    filename = "JVET-R2001-vB.pdf"
    print("Opening file " + filename)
    input = PdfFileReader(open(filename, "rb"))

    print(f"{filename} has {input.getNumPages()} pages.")

    

if __name__ == "__main__":
    main()