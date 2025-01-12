import sys

from pdfrw import PdfWriter
from pdfrw.objects.pdfname import PdfName
from pdfrw.objects.pdfstring import PdfString
from pdfrw.objects.pdfdict import PdfDict
from pdfrw.objects.pdfarray import PdfArray

def add_js(page, js):
  action = PdfDict()
  action.S = PdfName.JavaScript
  action.JS = js
  
  page.AA = PdfDict()
  page.AA.O = action

def create_page():
  page = PdfDict()
  page.Type = PdfName.Page

  page.MediaBox = PdfArray([0, 0, 500, 500])
  page.Contents = PdfDict()
  return page

if __name__ == "__main__":
  with open(sys.argv[1]) as f:
    js = f.read()

  writer = PdfWriter()
  page = create_page()
  add_js(page, js)

  writer.addpage(page)
  writer.write(sys.argv[2])