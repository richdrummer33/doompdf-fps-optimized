import sys
import pypdf

writer = pypdf.PdfWriter()
page = writer.add_blank_page(width=1000, height=1000)

with open(sys.argv[1]) as f:
  writer.add_js(f.read())
with open(sys.argv[2], "wb") as f:
  writer.write(f)