#!/usr/bin/env python3
"""Optional regeneration of bundled project-owned fixtures; requires pypdf + cryptography.
Fixed passwords are public test data, never application credentials.
"""
import pathlib
import sys
from pypdf import PdfReader, PdfWriter
from pypdf.constants import UserAccessPermissions as Permission
from pypdf.generic import DictionaryObject, NameObject, ArrayObject, NumberObject, TextStringObject
source=pathlib.Path(sys.argv[1])
output=pathlib.Path(sys.argv[2])
output.mkdir(parents=True,exist_ok=True)
profiles=[('restricted.pdf','',Permission.FILL_FORM_FIELDS),
          ('password.pdf','test-user',Permission.PRINT|Permission.EXTRACT),
          ('edit-no-extraction.pdf','',Permission.MODIFY),
          ('read-only-copy.pdf','',Permission.EXTRACT|Permission.PRINT),
          ('low-quality-print.pdf','',Permission.PRINT)]
for name,user,permissions in profiles:
    writer=PdfWriter(clone_from=PdfReader(source))
    if name=='restricted.pdf':
        field=DictionaryObject({NameObject('/Type'):NameObject('/Annot'),NameObject('/Subtype'):NameObject('/Widget'),
            NameObject('/FT'):NameObject('/Tx'),NameObject('/T'):TextStringObject('AllowedForm'),
            NameObject('/Rect'):ArrayObject([NumberObject(20),NumberObject(20),NumberObject(100),NumberObject(40)]),
            NameObject('/V'):TextStringObject('')})
        reference=writer._add_object(field)
        writer.pages[0][NameObject('/Annots')]=ArrayObject([reference])
        writer._root_object[NameObject('/AcroForm')]=DictionaryObject({NameObject('/Fields'):ArrayObject([reference]),
            NameObject('/DA'):TextStringObject('/Helv 12 Tf 0 g')})
    writer.encrypt(user_password=user,owner_password='test-owner',permissions_flag=permissions,algorithm='AES-128')
    with (output/name).open('wb') as stream:writer.write(stream)

# Explicit appearances isolate print-flag behavior from font/appearance generation.
from pypdf.generic import DecodedStreamObject
writer=PdfWriter(clone_from=PdfReader(source))
annotations=ArrayObject()
for x,flags,color in [(20,0,b"0 1 0"),(110,36,b"0 0 1")]:
    appearance=DecodedStreamObject()
    appearance.set_data(color+b" rg 0 0 60 60 re f\n")
    appearance.update({NameObject('/Type'):NameObject('/XObject'),NameObject('/Subtype'):NameObject('/Form'),
                       NameObject('/BBox'):ArrayObject([NumberObject(0),NumberObject(0),NumberObject(60),NumberObject(60)]),
                       NameObject('/Resources'):DictionaryObject()})
    form=writer._add_object(appearance)
    annotation=DictionaryObject({NameObject('/Type'):NameObject('/Annot'),NameObject('/Subtype'):NameObject('/Square'),
        NameObject('/Rect'):ArrayObject([NumberObject(x),NumberObject(20),NumberObject(x+60),NumberObject(80)]),
        NameObject('/F'):NumberObject(flags),NameObject('/AP'):DictionaryObject({NameObject('/N'):form})})
    annotations.append(writer._add_object(annotation))
writer.pages[0][NameObject('/Annots')]=annotations
with (output/'print-flags.pdf').open('wb') as stream:writer.write(stream)
