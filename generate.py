import sys
import textwrap

from pdfrw import PdfWriter
from pdfrw.objects.pdfname import PdfName
from pdfrw.objects.pdfstring import PdfString
from pdfrw.objects.pdfdict import PdfDict
from pdfrw.objects.pdfarray import PdfArray

def create_script(js):
  action = PdfDict()
  action.S = PdfName.JavaScript
  action.JS = js
  return action
  
def create_page(width, height):
  page = PdfDict()
  page.Type = PdfName.Page
  page.MediaBox = PdfArray([0, 0, width, height])

  page.Resources = PdfDict()
  page.Resources.Font = PdfDict()
  page.Resources.Font.F1 = PdfDict()
  page.Resources.Font.F1.Type = PdfName.Font
  page.Resources.Font.F1.Subtype = PdfName.Type1
  page.Resources.Font.F1.BaseFont = PdfName.Courier
  
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

  #r, g, b = color
  #appearance.stream = textwrap.dedent(f"""
  #  {r} {g} {b} rg
  #  0.0 0.0 {width} {height} re f
  #""")

  #annotation.AP = PdfDict()
  #annotation.AP.N = appearance
  #annotation.MK = PdfDict()
  #annotation.MK.BG = PdfArray(color)

  return annotation

if __name__ == "__main__":
  with open(sys.argv[1]) as f:
    js = f.read()

  width = 360
  height = 200
  scale = 2

  writer = PdfWriter()
  page = create_page(width * scale, height * scale + 250)
  page.AA = PdfDict()
  page.AA.O = create_script("try {"+js+"} catch (e) {app.alert(e.stack)}");

  fields = []
  for i in range(0, height):
    field = create_field(f"field_{i}", 0, i*scale + 250, width*scale, scale, [255, 255, 255], "")
    fields.append(field)
  for i in range(0, 28):
    field = create_field(f"console_{i}", 8, 8 + i*8, 300, 8, [255, 255, 255], "")
    fields.append(field)

  input_field = create_field(f"key_input", 320, 216, 60, 16, [255, 255, 255], "type here")
  input_field.AA = PdfDict()
  input_field.AA.K = create_script("key_pressed(event)")
  fields.append(input_field)
  page.Annots = PdfArray(fields)

  page2 = create_page(width * scale, height * scale)

  writer.addpage(page)
  writer.write(sys.argv[2])