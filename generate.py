import sys
import textwrap

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

def create_page(width, height):
  page = PdfDict()
  page.Type = PdfName.Page

  page.MediaBox = PdfArray([0, 0, width, height])
  #page.Contents = PdfDict()
  return page

def create_field(name, x, y, width, height, color, value=""):
  annotation = PdfDict()
  annotation.Type = PdfName.Annot
  annotation.Subtype = PdfName.Widget
  annotation.FT = PdfName.Tx
  annotation.Ff = 2
  annotation.Rect = PdfArray([x, y, x + width, y + height])
  annotation.T = PdfString.encode(name)
  annotation.V = PdfString.encode(value)

  annotation.BS = PdfDict()
  annotation.BS.W = 0

  appearance = PdfDict()
  appearance.Type = PdfName.XObject
  appearance.SubType = PdfName.Form
  appearance.FormType = 1
  appearance.BBox = PdfArray([0, 0, width, height])
  appearance.Matrix = PdfArray([1.0, 0.0, 0.0, 1.0, 0.0, 0.0])

  r, g, b = color
  appearance.stream = textwrap.dedent(f"""
    {r} {g} {b} rg
    0.0 0.0 {width} {height} re f
  """)

  annotation.AP = PdfDict()
  annotation.AP.N = appearance
  annotation.MK = PdfDict()
  annotation.MK.BG = PdfArray(color)

  return annotation

if __name__ == "__main__":
  with open(sys.argv[1]) as f:
    js = f.read()

  writer = PdfWriter()
  width = 360
  height = 200
  scale = 2
  page = create_page(width * scale, height * scale)
  add_js(page, js)

  #field = create_field("test", 10, 10, 20, 20, [0, 0, 0], "")

  fields = []

  for i in range(0, height):
    field = create_field(f"field_{i}", 0, i*scale, width*scale, scale, [255, 255, 255], "")
    fields.append(field)

  """

  color = [0, 0, 0]
  for y in range(0, height):
    for x in range(0, width):
      name = f"{x},{y}"
      fields.append(create_field(name, x*scale, y*scale, scale, scale, color))
  """
  page.Annots = PdfArray(fields)

  writer.addpage(page)
  writer.write(sys.argv[2])