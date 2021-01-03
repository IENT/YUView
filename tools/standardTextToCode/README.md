# Parse text to code

This tool is meant as a helper to convert the JEVC standard text word documents into CPP code. This code will not 100% compile but it can help a lot so that you don't have to type everything by hand (which may make you insane).

There are two tools:

### readStandardTextDocxFile.py

Read the `docx` document and parse all the tables (e.g `VPS`, `SPS`, ...). Put them all in a data structure and save them in a picled file.

### writeTablesC++.py

Take the pickled file and write CPP classes from it. This can be used as the starting point to create the actual parsing code. Or it can be used to add new parsing code which was added in a later version of a standard.
